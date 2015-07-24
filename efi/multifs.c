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

static EFI_HANDLE *logical_parts = NULL;
static unsigned int logical_parts_no = 0;

/* Find all device handles which support EFI_BLOCK_IO_PROTOCOL and are logical
 * partitions */
static EFI_STATUS find_all_logical_parts(void)
{
    EFI_STATUS status;
    unsigned long len = 0;
    EFI_HANDLE *handles = NULL;
    unsigned long i;
    EFI_BLOCK_IO *bio;

    if (logical_parts) {
        status = EFI_SUCCESS;
        goto out;
    }

    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, &len, NULL);
    if (EFI_ERROR(status) && status != EFI_BUFFER_TOO_SMALL)
        goto out;

    handles = malloc(len);
    if (!handles) {
        status = EFI_OUT_OF_RESOURCES;
        goto out;
    }

    logical_parts = malloc(len);
    if (!logical_parts) {
        status = EFI_OUT_OF_RESOURCES;
        goto out_free;
    }

    status = uefi_call_wrapper(BS->LocateHandle, 5, ByProtocol,
                               &BlockIoProtocol, NULL, &len,
                               (void **)handles);
    if (EFI_ERROR(status))
        goto out_free;

    for (i = 0; i < len / sizeof(EFI_HANDLE); i++) {
        status = uefi_call_wrapper(BS->HandleProtocol, 3, handles[i],
                                   &BlockIoProtocol, (void **)&bio);
        if (EFI_ERROR(status))
            goto out_free;
        if (bio->Media->LogicalPartition) {
            logical_parts[logical_parts_no++] = handles[i];
        }
    }

    free(handles);
    return status;

out_free:
    if (handles)
        free(handles);
    if (logical_parts)
        free(logical_parts);
out:
    return status;
}

static inline EFI_HANDLE get_logical_part(unsigned int partno)
{
    if (!logical_parts || partno > logical_parts_no)
        return NULL;
    return logical_parts[partno - 1];
}

static inline EFI_HANDLE find_device_handle(unsigned int diskno,
                                            unsigned int partno)
{
    return get_logical_part(partno);
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
    dprintf("%s: found partition %d\n", __func__, partno);

    priv = get_dev_info_priv(handle);
    if (!priv)
        goto free_fsp;

    ret = multifs_setup_fs_info(fsp, diskno, partno, priv);
    if (ret) {
        dprintf("%s: failed to set up fs info\n", __func__);
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

    status = find_all_logical_parts();
    if (EFI_ERROR(status)) {
        printf("%s: failed to locate device handles of logical partitions\n",
               __func__);
        printf("%s: EFI status code: 0x%08X\n", __func__, status);
        return;
    }
    dprintf("%s: initialised multifs support\n", __func__);
}
