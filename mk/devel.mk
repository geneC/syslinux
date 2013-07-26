# Useful while doing development, but not for production.
GCCWARN += -Wno-clobbered 
#GCCWARN += -DDEBUG_MALLOC
# GCCWARN += -DDEBUG_PORT=0x3f8 -DCORE_DEBUG=1
GCCWARN += -DDYNAMIC_DEBUG

## The following will enable printing ethernet/arp/ip/icmp/tcp/udp headers
##	in undiif.c
# GCCWARN += -DUNDIIF_ID_DEBUG=0x80U -DLWIP_DEBUG -DDEBUG_PORT=0x3f8
