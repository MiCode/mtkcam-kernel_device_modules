#ifndef LOCAL_DEFINES_H
#define LOCAL_DEFINES_H

#if __has_include("dt-bindings/memory/mt6991-larb-port.h")
#define IMGSYS_TF_DUMP_8_L
#endif

#if __has_include("dt-bindings/memory/mt6899-larb-port.h")
#define IMGSYS_TF_DUMP_8_P
#endif

#endif  /* LOCAL_DEFINES_H */
