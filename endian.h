#ifndef ENDIAN_H
#define ENDIAN_H

#ifndef __BYTE_ORDER__
  #warning "Compiler did not define __BYTE_ORDER__, assuming little endian"
  #ifndef __ORDER_LITTLE_ENDIAN__
    #define __ORDER_LITTLE_ENDIAN__ 1234
  #endif
  #define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#endif

#endif /* ENDIAN_H */
