/*
 * File:    avrnacl.h
 * Author:  Michael Hutter, Peter Schwabe
 * Version: Tue Aug 12 08:23:16 2014 +0200
 * Public Domain
 */

#ifndef AVRNACL_H
#define AVRNACL_H
#define AVRNACL_VERSION "2014-07-XXX"

// Change compared to original avrnacl: using standard int types.
#include <stdint.h>

typedef int8_t crypto_int8;
typedef uint8_t crypto_uint8;
typedef int16_t crypto_int16;
typedef uint16_t crypto_uint16;
typedef int32_t crypto_int32;
typedef uint32_t crypto_uint32;
typedef int64_t crypto_int64;
typedef uint64_t crypto_uint64;

#define crypto_auth_PRIMITIVE "hmacsha512256"
#define crypto_auth crypto_auth_hmacsha512256
#define crypto_auth_verify crypto_auth_hmacsha512256_verify
#define crypto_auth_BYTES crypto_auth_hmacsha512256_BYTES
#define crypto_auth_KEYBYTES crypto_auth_hmacsha512256_KEYBYTES
#define crypto_auth_hmacsha512256_BYTES 32
#define crypto_auth_hmacsha512256_KEYBYTES 32
extern int crypto_auth_hmacsha512256(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *);
extern int crypto_auth_hmacsha512256_verify(const unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *);

// Change compared to original avrnacl: removed all unused functions and
// definitions.
/*
#define crypto_box_PRIMITIVE "curve25519xsalsa20poly1305"
#define crypto_box crypto_box_curve25519xsalsa20poly1305
#define crypto_box_open crypto_box_curve25519xsalsa20poly1305_open
#define crypto_box_keypair crypto_box_curve25519xsalsa20poly1305_keypair
#define crypto_box_beforenm crypto_box_curve25519xsalsa20poly1305_beforenm
#define crypto_box_afternm crypto_box_curve25519xsalsa20poly1305_afternm
#define crypto_box_open_afternm crypto_box_curve25519xsalsa20poly1305_open_afternm
#define crypto_box_PUBLICKEYBYTES crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES
#define crypto_box_SECRETKEYBYTES crypto_box_curve25519xsalsa20poly1305_SECRETKEYBYTES
#define crypto_box_BEFORENMBYTES crypto_box_curve25519xsalsa20poly1305_BEFORENMBYTES
#define crypto_box_NONCEBYTES crypto_box_curve25519xsalsa20poly1305_NONCEBYTES
#define crypto_box_ZEROBYTES crypto_box_curve25519xsalsa20poly1305_ZEROBYTES
#define crypto_box_BOXZEROBYTES crypto_box_curve25519xsalsa20poly1305_BOXZEROBYTES
#define crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_SECRETKEYBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_BEFORENMBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_NONCEBYTES 24
#define crypto_box_curve25519xsalsa20poly1305_ZEROBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_BOXZEROBYTES 16
extern int crypto_box_curve25519xsalsa20poly1305(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_open(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_keypair(unsigned char *,unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_beforenm(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_afternm(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_open_afternm(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);

#define crypto_core_PRIMITIVE "salsa20"
#define crypto_core crypto_core_salsa20
#define crypto_core_OUTPUTBYTES crypto_core_salsa20_OUTPUTBYTES
#define crypto_core_INPUTBYTES crypto_core_salsa20_INPUTBYTES
#define crypto_core_KEYBYTES crypto_core_salsa20_KEYBYTES
#define crypto_core_CONSTBYTES crypto_core_salsa20_CONSTBYTES
#define crypto_core_salsa20_OUTPUTBYTES 64
#define crypto_core_salsa20_INPUTBYTES 16
#define crypto_core_salsa20_KEYBYTES 32
#define crypto_core_salsa20_CONSTBYTES 16
extern int crypto_core_salsa20(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);

#define crypto_core_hsalsa20_OUTPUTBYTES 32
#define crypto_core_hsalsa20_INPUTBYTES 16
#define crypto_core_hsalsa20_KEYBYTES 32
#define crypto_core_hsalsa20_CONSTBYTES 16
extern int crypto_core_hsalsa20(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);
*/

#define crypto_hashblocks_PRIMITIVE "sha512"
#define crypto_hashblocks crypto_hashblocks_sha512
#define crypto_hashblocks_STATEBYTES crypto_hashblocks_sha512_STATEBYTES
#define crypto_hashblocks_BLOCKBYTES crypto_hashblocks_sha512_BLOCKBYTES
#define crypto_hashblocks_sha512_STATEBYTES 64
#define crypto_hashblocks_sha512_BLOCKBYTES 128
extern int crypto_hashblocks_sha512(unsigned char *,const unsigned char *,crypto_uint16);

#define crypto_hash_PRIMITIVE "sha512"
#define crypto_hash crypto_hash_sha512
#define crypto_hash_BYTES crypto_hash_sha512_BYTES
#define crypto_hash_sha512_BYTES 64
extern int crypto_hash_sha512(unsigned char *,const unsigned char *,crypto_uint16);

/*
#define crypto_onetimeauth_PRIMITIVE "poly1305"
#define crypto_onetimeauth crypto_onetimeauth_poly1305
#define crypto_onetimeauth_verify crypto_onetimeauth_poly1305_verify
#define crypto_onetimeauth_BYTES crypto_onetimeauth_poly1305_BYTES
#define crypto_onetimeauth_KEYBYTES crypto_onetimeauth_poly1305_KEYBYTES
#define crypto_onetimeauth_poly1305_BYTES 16
#define crypto_onetimeauth_poly1305_KEYBYTES 32
extern int crypto_onetimeauth_poly1305(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *);
extern int crypto_onetimeauth_poly1305_verify(const unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *);

#define crypto_scalarmult_PRIMITIVE "curve25519"
#define crypto_scalarmult crypto_scalarmult_curve25519
#define crypto_scalarmult_base crypto_scalarmult_curve25519_base
#define crypto_scalarmult_BYTES crypto_scalarmult_curve25519_BYTES
#define crypto_scalarmult_SCALARBYTES crypto_scalarmult_curve25519_SCALARBYTES
#define crypto_scalarmult_curve25519_BYTES 32
#define crypto_scalarmult_curve25519_SCALARBYTES 32
extern int crypto_scalarmult_curve25519(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_scalarmult_curve25519_base(unsigned char *,const unsigned char *);

#define crypto_dh_PRIMITIVE "curve25519"
#define crypto_dh crypto_dh_curve25519
#define crypto_dh_keypair crypto_dh_curve25519_keypair
#define crypto_dh_BYTES crypto_dh_curve25519_BYTES
#define crypto_dh_SECRETKEYBYTES crypto_dh_curve25519_SECRETKEYBYTES
#define crypto_dh_PUBLICKEYBYTES crypto_dh_curve25519_PUBLICKEYBYTES
#define crypto_dh_BYTES crypto_dh_curve25519_BYTES
#define crypto_dh_curve25519_SECRETKEYBYTES 32
#define crypto_dh_curve25519_PUBLICKEYBYTES 32
#define crypto_dh_curve25519_BYTES 32
extern int crypto_dh_curve25519(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_dh_curve25519_keypair(unsigned char *,unsigned char *);

#define crypto_secretbox_PRIMITIVE "xsalsa20poly1305"
#define crypto_secretbox crypto_secretbox_xsalsa20poly1305
#define crypto_secretbox_open crypto_secretbox_xsalsa20poly1305_open
#define crypto_secretbox_KEYBYTES crypto_secretbox_xsalsa20poly1305_KEYBYTES
#define crypto_secretbox_NONCEBYTES crypto_secretbox_xsalsa20poly1305_NONCEBYTES
#define crypto_secretbox_ZEROBYTES crypto_secretbox_xsalsa20poly1305_ZEROBYTES
#define crypto_secretbox_BOXZEROBYTES crypto_secretbox_xsalsa20poly1305_BOXZEROBYTES
#define crypto_secretbox_xsalsa20poly1305_KEYBYTES 32
#define crypto_secretbox_xsalsa20poly1305_NONCEBYTES 24
#define crypto_secretbox_xsalsa20poly1305_ZEROBYTES 32
#define crypto_secretbox_xsalsa20poly1305_BOXZEROBYTES 16
extern int crypto_secretbox_xsalsa20poly1305(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);
extern int crypto_secretbox_xsalsa20poly1305_open(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);

#define crypto_sign_PRIMITIVE "ed25519"
#define crypto_sign crypto_sign_ed25519
#define crypto_sign_open crypto_sign_ed25519_open
#define crypto_sign_keypair crypto_sign_ed25519_keypair
#define crypto_sign_BYTES crypto_sign_ed25519_BYTES
#define crypto_sign_PUBLICKEYBYTES crypto_sign_ed25519_PUBLICKEYBYTES
#define crypto_sign_SECRETKEYBYTES crypto_sign_ed25519_SECRETKEYBYTES
#define crypto_sign_ed25519_BYTES 64
#define crypto_sign_ed25519_PUBLICKEYBYTES 32
#define crypto_sign_ed25519_SECRETKEYBYTES 64
extern int crypto_sign_ed25519(unsigned char *,crypto_uint16 *,const unsigned char *,crypto_uint16,const unsigned char *);
extern int crypto_sign_ed25519_open(unsigned char *,crypto_uint16 *,const unsigned char *,crypto_uint16,const unsigned char *);
extern int crypto_sign_ed25519_keypair(unsigned char *,unsigned char *);

#define crypto_stream_PRIMITIVE "xsalsa20"
#define crypto_stream crypto_stream_xsalsa20
#define crypto_stream_xor crypto_stream_xsalsa20_xor
#define crypto_stream_KEYBYTES crypto_stream_xsalsa20_KEYBYTES
#define crypto_stream_NONCEBYTES crypto_stream_xsalsa20_NONCEBYTES
#define crypto_stream_xsalsa20_KEYBYTES 32
#define crypto_stream_xsalsa20_NONCEBYTES 24
extern int crypto_stream_xsalsa20(unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_xor(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);

#define crypto_stream_salsa20_KEYBYTES 32
#define crypto_stream_salsa20_NONCEBYTES 8
extern int crypto_stream_salsa20(unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);
extern int crypto_stream_salsa20_xor(unsigned char *,const unsigned char *,crypto_uint16,const unsigned char *,const unsigned char *);

#define crypto_verify_PRIMITIVE "16"
#define crypto_verify crypto_verify_16
#define crypto_verify_BYTES crypto_verify_16_BYTES
#define crypto_verify_16_BYTES 16
extern int crypto_verify_16(const unsigned char *,const unsigned char *);

#define crypto_verify_32_BYTES 32
extern int crypto_verify_32(const unsigned char *,const unsigned char *);
*/

#endif
