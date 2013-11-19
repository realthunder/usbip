#undef USBIP_STRUCT_BEGIN
#undef USBIP_STRUCT_MEMBER_U32
#undef USBIP_STRUCT_MEMBER_S32
#undef USBIP_STRUCT_MEMBER_U16
#undef USBIP_STRUCT_MEMBER_S16
#undef USBIP_STRUCT_MEMBER_STRUCT
#undef USBIP_STRUCT_MEMBER_ARRAY
#undef USBIP_STRUCT_MEMBER_STRUCT_ARRAY
#undef USBIP_STRUCT_MEMBER_BYPASS
#undef USBIP_STRUCT_END

#ifdef USBIP_STRUCT_PACK
#   define USBIP_STRUCT_BEGIN(_name) static void inline pack_##_name(struct _name *s) {
#   ifndef __KERNEL__
#       define cpu_to_be32  htonl
#       define cpu_to_be16  htons
#   endif
#   define USBIP_STRUCT_MEMBER_U32(_name) s->_name = cpu_to_be32(s->_name)
#   define USBIP_STRUCT_MEMBER_U16(_name) s->_name = cpu_to_be16(s->_name)
#   define USBIP_STRUCT_MEMBER_S32(_name) USBIP_STRUCT_MEMBER_U32(_name)
#   define USBIP_STRUCT_MEMBER_S16(_name) USBIP_STRUCT_MEMBER_U16(_name)
#   define USBIP_STRUCT_MEMBER_STRUCT(_type,_name) pack_##_type(&s->_name)
#   define USBIP_STRUCT_END }
#elif defined(USBIP_STRUCT_UNPACK)
#   define USBIP_STRUCT_BEGIN(_name) static void inline unpack_##_name(struct _name *s) {
#   ifndef __KERNEL__
#       define be32_to_cpu  ntohl
#       define be16_to_cpu  ntohs
#   endif
#   define USBIP_STRUCT_MEMBER_U32(_name) s->_name = be32_to_cpu(s->_name)
#   define USBIP_STRUCT_MEMBER_U16(_name) s->_name = be16_to_cpu(s->_name)
#   define USBIP_STRUCT_MEMBER_S32(_name) USBIP_STRUCT_MEMBER_U32(_name)
#   define USBIP_STRUCT_MEMBER_S16(_name) USBIP_STRUCT_MEMBER_U16(_name)
#   define USBIP_STRUCT_MEMBER_STRUCT(_type,_name) unpack_##_type(&s->_name)
#   define USBIP_STRUCT_END }
#else
#   ifdef _MSC_VER
#       define USBIP_STRUCT_BEGIN(_name) __pragma(pack(push,1)) struct _name {
#   else
#       define USBIP_STRUCT_BEGIN(_name) struct _name {
#   endif
#   define USBIP_STRUCT_MEMBER_U32(_name) uint32_t _name
#   define USBIP_STRUCT_MEMBER_S32(_name) int32_t _name
#   define USBIP_STRUCT_MEMBER_U16(_name) uint16_t _name
#   define USBIP_STRUCT_MEMBER_S16(_name) int16_t _name
#   define USBIP_STRUCT_MEMBER_STRUCT(_type,_name) struct _type _name
#   define USBIP_STRUCT_MEMBER_BYPASS(_name) _name
#   define USBIP_STRUCT_MEMBER_ARRAY(_type,_name,_size) USBIP_STRUCT_MEMBER_##_type(_name[_size])
#   define USBIP_STRUCT_MEMBER_STRUCT_ARRAY(_type,_name,_size) USBIP_STRUCT_MEMBER_STRUCT(type,_name[_size])
#   ifdef _MSC_VER
#       define USBIP_STRUCT_END } __pragma(pack(pop));
#   else
#       define USBIP_STRUCT_END } __attribute__((packed));
#   endif
#endif

#if defined(USBIP_STRUCT_PACK) || defined(USBIP_STRUCT_UNPACK)
#   define USBIP_STRUCT_MEMBER_ARRAY(_type,_name,_size) \
    do{\
        size_t i;\
        for(i=0;i<_size;++i) \
            USBIP_STRUCT_MEMBER_##_type(_name[i]);\
    }while(0)
#   define USBIP_STRUCT_MEMBER_STRUCT_ARRAY(_type,_name,_size) \
    do{\
        size_t i;\
        for(i=0;i<_size;++i) \
            USBIP_STRUCT_MEMBER_STRUCT(_type,_name[i]);\
    }while(0)
#   define USBIP_STRUCT_MEMBER_BYPASS(_name)
#endif

