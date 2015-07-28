/*
 * Copyright (c) 2015 Paulo Alcantara <pcacjr@zytor.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#include <fs.h>
#include <multifs.h>

#include "efi.h"

#define DISKS_MAX 0xff
#define PARTS_MAX 0xff

struct efi_disk_entry {
    uint8_t sig_type;
    union {
        uint32_t id;
        EFI_GUID guid;
    };
    EFI_HANDLE parts[PARTS_MAX];
};

static uint8_t disks_no = 0;
static struct efi_disk_entry efi_disks[DISKS_MAX] = { 0, };

/* Find all device handles that support EFI_BLOCK_IO_PROTOCOL */
static EFI_STATUS find_all_block_devs(EFI_HANDLE **bdevs, unsigned long *len)
{
    EFI_STATUS status;

    *len = 0;
    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, len, NULL);
    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL) {
        Print(L"%a: failed to locate BlockIo handles\n", __func__);
        goto out;
    }

    *bdevs = malloc(*len);
    if (!*bdevs) {
        Print(L"%a: malloc failed\n", __func__);
        status = EFI_OUT_OF_RESOURCES;
        goto out;
    }

    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, len,
                               (void **)*bdevs);
    if (EFI_ERROR(status)) {
        Print(L"%a: failed to locate BlockIo handles\n", __func__);
        goto out_free;
    }
    return status;

out_free:
    free(*bdevs);
out:
    return status;
}

static inline PCI_DEVICE_PATH *get_pci_dev_path(EFI_HANDLE dev)
{
    EFI_DEVICE_PATH *path;

    for (path = DevicePathFromHandle(dev); !IsDevicePathEndType(path);
         path = NextDevicePathNode(path)) {
        if (DevicePathType(path) == HARDWARE_DEVICE_PATH &&
            DevicePathSubType(path) == HW_PCI_DP) {
            return (PCI_DEVICE_PATH *)path;
        }
    }
    return NULL;
}

static inline HARDDRIVE_DEVICE_PATH *get_hd_dev_path(EFI_HANDLE dev)
{
    EFI_DEVICE_PATH *path;

    for (path = DevicePathFromHandle(dev); !IsDevicePathEndType(path);
         path = NextDevicePathNode(path)) {
        if (DevicePathType(path) == MEDIA_DEVICE_PATH &&
            DevicePathSubType(path) == MEDIA_HARDDRIVE_DP) {
            return (HARDDRIVE_DEVICE_PATH *)path;
        }
    }
    return NULL;
}

static int set_efi_disk_info(uint8_t di, EFI_HANDLE dev)
{
    HARDDRIVE_DEVICE_PATH *hd;

    hd = get_hd_dev_path(dev);
    if (!hd)
        return -1;

    switch (hd->SignatureType) {
    case SIGNATURE_TYPE_MBR:
        efi_disks[di].id = *((uint32_t *)&hd->Signature[0]);
        break;
    case SIGNATURE_TYPE_GUID:
        memcpy(&efi_disks[di].guid, &hd->Signature[0],
               sizeof(efi_disks[di].guid));
        break;
    }
    efi_disks[di].sig_type = hd->SignatureType;
    return 0;
}

static EFI_STATUS find_rem_parts(uint8_t di, EFI_HANDLE *bdevs,
                                 unsigned long *bi, unsigned long bdevsno)
{
    unsigned long i = *bi + 1;
    EFI_STATUS status = EFI_SUCCESS;
    EFI_BLOCK_IO *bio;
    PCI_DEVICE_PATH *dev_pci = get_pci_dev_path(bdevs[*bi]);
    PCI_DEVICE_PATH *pci;
    HARDDRIVE_DEVICE_PATH *hd;
    uint8_t pi = 1;

    while (i < bdevsno) {
        pci = get_pci_dev_path(bdevs[i]);
        if (dev_pci->Function != pci->Function &&
            dev_pci->Device != pci->Device) {
            break;
        }
        status = uefi_call_wrapper(BS->HandleProtocol, 3, bdevs[i],
                                   &BlockIoProtocol, (void **)&bio);
        if (EFI_ERROR(status)) {
            Print(L"%a: failed to find BlockIO protocol: %r\n", __func__,
                  status);
            break;
        }
        if (!bio->Media->LogicalPartition)
            break;
        hd = get_hd_dev_path(bdevs[i]);
        if (hd->SignatureType != efi_disks[di].sig_type)
            break;

        efi_disks[di].parts[pi++] = bdevs[i++];
    }
    *bi = i - 1;
    return status;
}

static EFI_STATUS find_all_partitions(void)
{
    EFI_STATUS status;
    EFI_HANDLE *bdevs;
    unsigned long len;
    unsigned long i;
    EFI_BLOCK_IO *bio;
    int ret;

    status = find_all_block_devs(&bdevs, &len);
    if (EFI_ERROR(status)) {
        Print(L"%a: failed to find block devices: %r\n", __func__, status);
        return status;
    }

    for (i = 0; i < len / sizeof(EFI_HANDLE); i++) {
        status = uefi_call_wrapper(BS->HandleProtocol, 3, bdevs[i],
                                   &BlockIoProtocol, (void **)&bio);
        if (EFI_ERROR(status))
            break;
        if (!bio->Media->LogicalPartition)
            continue;
        ret = set_efi_disk_info(disks_no, bdevs[i]);
        if (ret) {
            Print(L"%a: failed to set EFI disk info\n", __func__);
            status = EFI_INVALID_PARAMETER;
            break;
        }
        efi_disks[disks_no].parts[0] = bdevs[i]; /* first partition */
        status = find_rem_parts(disks_no, bdevs, &i, len / sizeof(EFI_HANDLE));
        if (EFI_ERROR(status)) {
            Print(L"%a: failed to find partitions: %r\n", __func__, status);
            break;
        }
        disks_no++;
    }
    free(bdevs);
    return status;
}

static inline EFI_HANDLE find_device_handle(uint8_t di, uint8_t pi)
{
    if (di >= disks_no || pi > PARTS_MAX)
        return NULL;
    return efi_disks[di].parts[pi - 1];
}

static inline void *get_dev_info_priv(EFI_HANDLE handle)
{
    struct efi_disk_private *priv;

    priv = malloc(sizeof(*priv));
    if (!priv)
        return NULL;
    priv->dev_handle = handle;
    return priv;
}

__export struct fs_info *efi_multifs_get_fs_info(const char **path)
{
    uint8_t diskno;
    uint8_t partno;
    struct fs_info *fsp;
    EFI_HANDLE handle;
    void *priv;
    int ret;

    if (multifs_parse_path(path, &diskno, &partno))
        return NULL;

    fsp = multifs_get_fs(diskno, partno - 1);
    if (fsp)
        return fsp;

    fsp = malloc(sizeof(*fsp));
    if (!fsp)
        return NULL;

    handle = find_device_handle(diskno, partno);
    if (!handle)
        goto free_fsp;
    dprintf("%s: found disk %u part %u\n", __func__, diskno, partno);

    priv = get_dev_info_priv(handle);
    if (!priv)
        goto free_fsp;

    ret = multifs_setup_fs_info(fsp, diskno, partno, priv);
    if (ret) {
        Print(L"%a: failed to set up fs info\n", __func__);
        goto free_priv;
    }

    return fsp;

free_priv:
    free(priv);
free_fsp:
    free(fsp);
    return NULL;
}

__export void efi_multifs_init(void *addr __attribute__((unused)))
{
    EFI_STATUS status;

    status = find_all_partitions();
    if (EFI_ERROR(status)) {
        Print(L"%a: failed to find disk partitions: %r\n", __func__, status);
        return;
    }
    dprintf("%s: initialised multifs support\n", __func__);
}
