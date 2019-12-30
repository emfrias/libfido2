/*
 * Copyright (c) 2019 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mutator_aux.h"
#include "fido.h"
#include "fido/bio.h"

#include "../openbsd-compat/openbsd-compat.h"

#define TAG_PIN			0x01
#define TAG_NAME		0x02
#define TAG_SEED		0x03
#define TAG_ID			0x04
#define TAG_INFO_WIRE_DATA	0x05
#define TAG_ENROLL_WIRE_DATA	0x06
#define TAG_LIST_WIRE_DATA	0x07
#define TAG_SET_NAME_WIRE_DATA	0x08
#define TAG_REMOVE_WIRE_DATA	0x09

/* Parameter set defining a FIDO2 credential management operation. */
struct param {
	char		pin[MAXSTR];
	char		name[MAXSTR];
	int		seed;
	struct blob	id;
	struct blob	info_wire_data;
	struct blob	enroll_wire_data;
	struct blob	list_wire_data;
	struct blob	set_name_wire_data;
	struct blob	remove_wire_data;
};

/* Example parameters. */
static const uint8_t dummy_id[] = { 0x5e, 0xd2, };
static const char dummy_pin[] = "3Q;I){TAx";
static const char dummy_name[] = "finger1";

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'getFingerprintSensorInfo' bio enrollment command.
 */
static const uint8_t dummy_info_wire_data[] = {
	/* CTAP_CMD_INIT */
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0xf0,
	0x08, 0xc1, 0x8f, 0x76, 0x4b, 0x8f, 0xa9, 0x00,
	0x10, 0x00, 0x04, 0x02, 0x00, 0x04, 0x06, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_GETINFO */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (get info) */
	0x00, 0x10, 0x00, 0x04, 0x90, 0x00, 0x06, 0x00,
	0xa2, 0x02, 0x01, 0x03, 0x04, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with FIDO2
 * 'enrollBegin' + 'enrollCaptureNextSample' bio enrollment commands.
 */
static const uint8_t dummy_enroll_wire_data[] = {
	/* CTAP_CMD_INIT */
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x06,
	0xb4, 0xba, 0x2e, 0xb3, 0x88, 0x24, 0x38, 0x00,
	0x0a, 0x00, 0x05, 0x02, 0x00, 0x04, 0x06, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_GETINFO */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 1) */
	0x00, 0x0a, 0x00, 0x05, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0xc9, 0x12, 0x01,
	0xab, 0x88, 0xd7, 0x0a, 0x24, 0xdd, 0xdc, 0xde,
	0x16, 0x27, 0x50, 0x77, 0x37, 0x06, 0xd3, 0x48,
	0xe6, 0xf9, 0xdb, 0xaa, 0x10, 0x83, 0x81, 0xac,
	0x13, 0x3c, 0xf9, 0x77, 0x2d, 0x22, 0x58, 0x20,
	0xda, 0x20, 0x71, 0x03, 0x01, 0x40, 0xac, 0xd0,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 2) */
	0x00, 0x0a, 0x00, 0x05, 0x00, 0xb8, 0xdf, 0x2a,
	0x95, 0xd3, 0x88, 0x1c, 0x06, 0x34, 0x30, 0xf1,
	0xf3, 0xcd, 0x27, 0x40, 0x90, 0x5c, 0xc6, 0x74,
	0x66, 0xff, 0x10, 0xde, 0xb6, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get pin token) */
	0x00, 0x0a, 0x00, 0x05, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x18, 0x81, 0xff, 0xf2, 0xf5,
	0xde, 0x74, 0x43, 0xd5, 0xe0, 0x77, 0x37, 0x6b,
	0x6c, 0x18, 0x6b, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll begin) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll begin) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll begin) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll begin) */
	0x00, 0x0a, 0x00, 0x05, 0x90, 0x00, 0x0a, 0x00,
	0xa3, 0x04, 0x42, 0x68, 0x96, 0x05, 0x00, 0x06,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll continue) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll continue) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll continue) */
	0x00, 0x0a, 0x00, 0x05, 0x90, 0x00, 0x06, 0x00,
	0xa2, 0x05, 0x00, 0x06, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll continue) */
	0x00, 0x0a, 0x00, 0x05, 0xbb, 0x00, 0x01, 0x02,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll continue) */
	0x00, 0x0a, 0x00, 0x05, 0x90, 0x00, 0x06, 0x00,
	0xa2, 0x05, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'enumerateEnrollments' bio enrollment command.
 */
static const uint8_t dummy_list_wire_data[] = {
	/* CTAP_CMD_INIT */
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0xae,
	0x21, 0x88, 0x51, 0x09, 0x6f, 0xd7, 0xbb, 0x00,
	0x10, 0x00, 0x0f, 0x02, 0x00, 0x04, 0x06, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_GETINFO */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 1) */
	0x00, 0x10, 0x00, 0x0f, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x5a, 0x70, 0x63,
	0x11, 0x5b, 0xa6, 0xe1, 0x8e, 0x4a, 0xb0, 0x75,
	0xe7, 0xfd, 0x39, 0x26, 0x29, 0xed, 0x69, 0xb0,
	0xc1, 0x1f, 0xa5, 0x7d, 0xcb, 0x64, 0x1e, 0x7c,
	0x9f, 0x60, 0x5e, 0xb2, 0xf8, 0x22, 0x58, 0x20,
	0xec, 0xe9, 0x1b, 0x11, 0xac, 0x2a, 0x0d, 0xd5,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 2) */
	0x00, 0x10, 0x00, 0x0f, 0x00, 0x3b, 0x9f, 0xba,
	0x0f, 0x25, 0xd5, 0x24, 0x33, 0x4c, 0x5d, 0x0f,
	0x63, 0xbf, 0xf1, 0xf3, 0x64, 0x55, 0x78, 0x1a,
	0x59, 0x6e, 0x65, 0x59, 0xfc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get pin token) */
	0x00, 0x10, 0x00, 0x0f, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0xb9, 0x31, 0x34, 0xe2, 0x71,
	0x6a, 0x8e, 0xa3, 0x60, 0xec, 0x5e, 0xd2, 0x13,
	0x2e, 0x19, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enum) */
	0x00, 0x10, 0x00, 0x0f, 0x90, 0x00, 0x2e, 0x00,
	0xa1, 0x07, 0x83, 0xa2, 0x01, 0x42, 0xce, 0xa3,
	0x02, 0x67, 0x66, 0x69, 0x6e, 0x67, 0x65, 0x72,
	0x31, 0xa2, 0x01, 0x42, 0xbf, 0x5e, 0x02, 0x67,
	0x66, 0x69, 0x6e, 0x67, 0x65, 0x72, 0x32, 0xa2,
	0x01, 0x42, 0x5e, 0xd2, 0x02, 0x67, 0x66, 0x69,
	0x6e, 0x67, 0x65, 0x72, 0x33, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'setFriendlyName' bio enrollment command.
 */
static const uint8_t dummy_set_name_wire_data[] = {
	/* CTAP_CMD_INIT */
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0xac,
	0x48, 0xfd, 0xbd, 0xdd, 0x36, 0x24, 0x4d, 0x00,
	0x10, 0x00, 0x10, 0x02, 0x00, 0x04, 0x06, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_GETINFO */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 1) */
	0x00, 0x10, 0x00, 0x10, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x5a, 0x70, 0x63,
	0x11, 0x5b, 0xa6, 0xe1, 0x8e, 0x4a, 0xb0, 0x75,
	0xe7, 0xfd, 0x39, 0x26, 0x29, 0xed, 0x69, 0xb0,
	0xc1, 0x1f, 0xa5, 0x7d, 0xcb, 0x64, 0x1e, 0x7c,
	0x9f, 0x60, 0x5e, 0xb2, 0xf8, 0x22, 0x58, 0x20,
	0xec, 0xe9, 0x1b, 0x11, 0xac, 0x2a, 0x0d, 0xd5,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 2) */
	0x00, 0x10, 0x00, 0x10, 0x00, 0x3b, 0x9f, 0xba,
	0x0f, 0x25, 0xd5, 0x24, 0x33, 0x4c, 0x5d, 0x0f,
	0x63, 0xbf, 0xf1, 0xf3, 0x64, 0x55, 0x78, 0x1a,
	0x59, 0x6e, 0x65, 0x59, 0xfc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get pin token) */
	0x00, 0x10, 0x00, 0x10, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0x40, 0x95, 0xf3, 0xcb, 0xae,
	0xf2, 0x8d, 0xd9, 0xe0, 0xe0, 0x8a, 0xbd, 0xc3,
	0x03, 0x58, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (set name) */
	0x00, 0x10, 0x00, 0x10, 0x90, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*
 * Collection of HID reports from an authenticator issued with a FIDO2
 * 'removeEnrollment' bio enrollment command.
 */
static const uint8_t dummy_remove_wire_data[] = {
	/* CTAP_CMD_INIT */
	0xff, 0xff, 0xff, 0xff, 0x86, 0x00, 0x11, 0x4b,
	0x24, 0xde, 0xd9, 0x06, 0x57, 0x1a, 0xbd, 0x00,
	0x10, 0x00, 0x15, 0x02, 0x00, 0x04, 0x06, 0x05,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_GETINFO */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 1) */
	0x00, 0x10, 0x00, 0x15, 0x90, 0x00, 0x51, 0x00,
	0xa1, 0x01, 0xa5, 0x01, 0x02, 0x03, 0x38, 0x18,
	0x20, 0x01, 0x21, 0x58, 0x20, 0x5a, 0x70, 0x63,
	0x11, 0x5b, 0xa6, 0xe1, 0x8e, 0x4a, 0xb0, 0x75,
	0xe7, 0xfd, 0x39, 0x26, 0x29, 0xed, 0x69, 0xb0,
	0xc1, 0x1f, 0xa5, 0x7d, 0xcb, 0x64, 0x1e, 0x7c,
	0x9f, 0x60, 0x5e, 0xb2, 0xf8, 0x22, 0x58, 0x20,
	0xec, 0xe9, 0x1b, 0x11, 0xac, 0x2a, 0x0d, 0xd5,
	/* CTAP_CBOR_CLIENT_PIN (get authenticator key; frame 2) */
	0x00, 0x10, 0x00, 0x15, 0x00, 0x3b, 0x9f, 0xba,
	0x0f, 0x25, 0xd5, 0x24, 0x33, 0x4c, 0x5d, 0x0f,
	0x63, 0xbf, 0xf1, 0xf3, 0x64, 0x55, 0x78, 0x1a,
	0x59, 0x6e, 0x65, 0x59, 0xfc, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_CLIENT_PIN (get pin token) */
	0x00, 0x10, 0x00, 0x15, 0x90, 0x00, 0x14, 0x00,
	0xa1, 0x02, 0x50, 0xb0, 0xd0, 0x71, 0x2f, 0xa7,
	0x8b, 0x89, 0xbd, 0xca, 0xa4, 0x1e, 0x6c, 0x43,
	0xa1, 0x71, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* CTAP_CBOR_BIO_ENROLL_PRE (enroll remove) (*/
	0x00, 0x10, 0x00, 0x15, 0x90, 0x00, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

int    LLVMFuzzerTestOneInput(const uint8_t *, size_t);
size_t LLVMFuzzerCustomMutator(uint8_t *, size_t, size_t, unsigned int);

static int
unpack(const uint8_t *ptr, size_t len, struct param *p) NO_MSAN
{
	uint8_t **pp = (void *)&ptr;

	if (unpack_string(TAG_PIN, pp, &len, p->pin) < 0 ||
	    unpack_string(TAG_NAME, pp, &len, p->name) < 0 ||
	    unpack_int(TAG_SEED, pp, &len, &p->seed) < 0 ||
	    unpack_blob(TAG_ID, pp, &len, &p->id) < 0 ||
	    unpack_blob(TAG_INFO_WIRE_DATA, pp, &len, &p->info_wire_data) < 0 ||
	    unpack_blob(TAG_ENROLL_WIRE_DATA, pp, &len, &p->enroll_wire_data) < 0 ||
	    unpack_blob(TAG_LIST_WIRE_DATA, pp, &len, &p->list_wire_data) < 0 ||
	    unpack_blob(TAG_SET_NAME_WIRE_DATA, pp, &len, &p->set_name_wire_data) < 0 ||
	    unpack_blob(TAG_REMOVE_WIRE_DATA, pp, &len, &p->remove_wire_data) < 0)
		return (-1);

	return (0);
}

static size_t
pack(uint8_t *ptr, size_t len, const struct param *p)
{
	const size_t max = len;

	if (pack_string(TAG_PIN, &ptr, &len, p->pin) < 0 ||
	    pack_string(TAG_NAME, &ptr, &len, p->name) < 0 ||
	    pack_int(TAG_SEED, &ptr, &len, p->seed) < 0 ||
	    pack_blob(TAG_ID, &ptr, &len, &p->id) < 0 ||
	    pack_blob(TAG_INFO_WIRE_DATA, &ptr, &len, &p->info_wire_data) < 0 ||
	    pack_blob(TAG_ENROLL_WIRE_DATA, &ptr, &len, &p->enroll_wire_data) < 0 ||
	    pack_blob(TAG_LIST_WIRE_DATA, &ptr, &len, &p->list_wire_data) < 0 ||
	    pack_blob(TAG_SET_NAME_WIRE_DATA, &ptr, &len, &p->set_name_wire_data) < 0 ||
	    pack_blob(TAG_REMOVE_WIRE_DATA, &ptr, &len, &p->remove_wire_data) < 0)
		return (0);

	return (max - len);
}

static size_t
input_len(int max)
{
	return (2 * len_string(max) + len_int() + 6 * len_blob(max));
}

static fido_dev_t *
prepare_dev()
{
	fido_dev_t	*dev;
	fido_dev_io_t	 io;

	memset(&io, 0, sizeof(io));

	io.open = dev_open;
	io.close = dev_close;
	io.read = dev_read;
	io.write = dev_write;

	if ((dev = fido_dev_new()) == NULL || fido_dev_set_io_functions(dev,
	    &io) != FIDO_OK || fido_dev_open(dev, "nodev") != FIDO_OK) {
		fido_dev_free(&dev);
		return (NULL);
	}

	return (dev);
}

static void
get_info(struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_info_t *i = NULL;
	uint8_t type;
	uint8_t max_samples;

	set_wire_data(p->info_wire_data.body, p->info_wire_data.len);

	if ((dev = prepare_dev()) == NULL || (i = fido_bio_info_new()) == NULL)
		goto done;

	fido_bio_dev_get_info(dev, i);

	type = fido_bio_info_type(i);
	max_samples = fido_bio_info_max_samples(i);
	consume(&type, sizeof(type));
	consume(&max_samples, sizeof(max_samples));

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_info_free(&i);
}

static void
consume_template(const fido_bio_template_t *t)
{
	consume(fido_bio_template_name(t), xstrlen(fido_bio_template_name(t)));
	consume(fido_bio_template_id_ptr(t), fido_bio_template_id_len(t));
}

static void
consume_enroll(fido_bio_enroll_t *e)
{
	uint8_t last_status;
	uint8_t remaining_samples;

	last_status = fido_bio_enroll_last_status(e);
	remaining_samples = fido_bio_enroll_remaining_samples(e);
	consume(&last_status, sizeof(last_status));
	consume(&remaining_samples, sizeof(remaining_samples));
}

static void
enroll(struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;
	fido_bio_enroll_t *e = NULL;
	size_t cnt = 0;

	set_wire_data(p->enroll_wire_data.body, p->enroll_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL ||
	    (e = fido_bio_enroll_new()) == NULL)
		goto done;

	fido_bio_dev_enroll_begin(dev, t, e, p->seed, p->pin);

	consume_template(t);
	consume_enroll(e);

	while (fido_bio_enroll_remaining_samples(e) > 0 && cnt++ < 5) {
		fido_bio_dev_enroll_continue(dev, t, e, p->seed);
		consume_template(t);
		consume_enroll(e);
	}

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
	fido_bio_enroll_free(&e);
}

static void
list(struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_array_t *ta = NULL;
	const fido_bio_template_t *t = NULL;

	set_wire_data(p->list_wire_data.body, p->list_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (ta = fido_bio_template_array_new()) == NULL)
		goto done;

	fido_bio_dev_get_template_array(dev, ta, p->pin);

	/* +1 on purpose */
	for (size_t i = 0; i < fido_bio_template_array_count(ta) + 1; i++)
		if ((t = fido_bio_template(ta, i)) != NULL)
			consume_template(t);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_array_free(&ta);
}

static void
set_name(struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;

	set_wire_data(p->set_name_wire_data.body, p->set_name_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL)
		goto done;

	fido_bio_template_set_name(t, p->name);
	fido_bio_template_set_id(t, p->id.body, p->id.len);
	consume_template(t);

	fido_bio_dev_set_template_name(dev, t, p->pin);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
}

static void
del(struct param *p)
{
	fido_dev_t *dev = NULL;
	fido_bio_template_t *t = NULL;

	set_wire_data(p->remove_wire_data.body, p->remove_wire_data.len);

	if ((dev = prepare_dev()) == NULL ||
	    (t = fido_bio_template_new()) == NULL)
		goto done;

	fido_bio_template_set_id(t, p->id.body, p->id.len);
	consume_template(t);

	fido_bio_dev_enroll_remove(dev, t, p->pin);

done:
	if (dev)
		fido_dev_close(dev);

	fido_dev_free(&dev);
	fido_bio_template_free(&t);
}

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct param p;

	memset(&p, 0, sizeof(p));

	if (size < input_len(GETLEN_MIN) || size > input_len(GETLEN_MAX) ||
	    unpack(data, size, &p) < 0)
		return (0);

	srandom((unsigned int)p.seed);

	fido_init(FIDO_DEBUG);
	fido_set_log_handler(consume_str);

	get_info(&p);
	enroll(&p);
	list(&p);
	set_name(&p);
	del(&p);

	return (0);
}

static size_t
pack_dummy(uint8_t *ptr, size_t len)
{
	struct param	dummy;
	uint8_t		blob[32768];
	size_t		blob_len;

	memset(&dummy, 0, sizeof(dummy));

	strlcpy(dummy.pin, dummy_pin, sizeof(dummy.pin));
	strlcpy(dummy.name, dummy_name, sizeof(dummy.name));

	dummy.info_wire_data.len = sizeof(dummy_info_wire_data);
	dummy.enroll_wire_data.len = sizeof(dummy_enroll_wire_data);
	dummy.list_wire_data.len = sizeof(dummy_list_wire_data);
	dummy.set_name_wire_data.len = sizeof(dummy_set_name_wire_data);
	dummy.remove_wire_data.len = sizeof(dummy_remove_wire_data);
	dummy.id.len = sizeof(dummy_id);

	memcpy(&dummy.info_wire_data.body, &dummy_info_wire_data,
	    dummy.info_wire_data.len);
	memcpy(&dummy.enroll_wire_data.body, &dummy_enroll_wire_data,
	    dummy.enroll_wire_data.len);
	memcpy(&dummy.list_wire_data.body, &dummy_list_wire_data,
	    dummy.list_wire_data.len);
	memcpy(&dummy.set_name_wire_data.body, &dummy_set_name_wire_data,
	    dummy.set_name_wire_data.len);
	memcpy(&dummy.remove_wire_data.body, &dummy_remove_wire_data,
	    dummy.remove_wire_data.len);
	memcpy(&dummy.id.body, &dummy_id, dummy.id.len);

	blob_len = pack(blob, sizeof(blob), &dummy);
	assert(blob_len != 0);

	if (blob_len > len) {
		memcpy(ptr, blob, len);
		return (len);
	}

	memcpy(ptr, blob, blob_len);

	return (blob_len);
}

size_t
LLVMFuzzerCustomMutator(uint8_t *data, size_t size, size_t maxsize,
    unsigned int seed) NO_MSAN
{
	struct param	p;
	uint8_t		blob[16384];
	size_t		blob_len;

	memset(&p, 0, sizeof(p));

	if (unpack(data, size, &p) < 0)
		return (pack_dummy(data, maxsize));

	p.seed = (int)seed;

	mutate_blob(&p.id);
	mutate_blob(&p.info_wire_data);
	mutate_blob(&p.enroll_wire_data);
	mutate_blob(&p.list_wire_data);
	mutate_blob(&p.set_name_wire_data);
	mutate_blob(&p.remove_wire_data);

	mutate_string(p.pin);
	mutate_string(p.name);

	blob_len = pack(blob, sizeof(blob), &p);

	if (blob_len == 0 || blob_len > maxsize)
		return (0);

	memcpy(data, blob, blob_len);

	return (blob_len);
}
