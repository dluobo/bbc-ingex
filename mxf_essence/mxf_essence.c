/*
 * $Id: mxf_essence.c,v 1.1.1.1 2006/04/30 08:38:05 stuart_hc Exp $
 *
 * Functions to support extraction of raw essence data out of MXF files.
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
 
 
#include <assert.h>
#include <unistd.h>
#include <inttypes.h>
#include <string.h>

#include <mxf_essence.h>


// keep this in sync with mxfe_EssenceType
static const struct 
{
    mxfe_EssenceType type;
    const char* suffix;
} essenceTypeSuffixes[] = 
{
    {MXFE_DV, "dv"},
    {MXFE_WAVPCM, "wavpcm"},
    {MXFE_AVIDMJPEG, "mjpeg"},
    {MXFE_UNKNOWN, "unknown"}
};

typedef struct 
{
	uint8_t _key[16];
} Key;
    

static const Key g_ppPrefixKey =
{{0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01}};


// closed and complete mxf header
static const Key g_headerPPKey =
{{0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x02, 0x04, 0x00}};

// closed and complete body partition
static const Key g_bodyPPKey =
{{0x06, 0x0e, 0x2b, 0x34, 0x02, 0x05, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x01, 0x03, 0x04, 0x00}};

static const Key g_klvFillKey =
{{0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x01, 0x03, 0x01, 0x02, 0x10, 0x01, 0x00, 0x00, 0x00}};


static const Key g_opAtomLabelMask = 
{{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0xff, 0xff}};

static const Key g_opAtomLabel = 
{{0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0d, 0x01, 0x02, 0x01, 0x10, 0x00, 0x00, 0x00}};

static const Key g_essenceContainerAAFLabel = 
{{0x80, 0x9b, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f, 0x43, 0x13, 0xb5, 0x71, 0xd8, 0xba, 0x11, 0xd2}};

static const Key g_essElementPrefixKey =
{{0x06, 0x0e, 0x2b, 0x34, 0x01, 0x02, 0x01, 0x01}};


static const Key g_BWFClipWrappedECLabel = 
{{0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0d, 0x01, 0x03, 0x01, 0x02, 0x06, 0x02, 0x00}};

static const Key g_DV50ClipWrappedECLabel = 
{{0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0d, 0x01, 0x03, 0x01, 0x02, 0x02, 0x51, 0x02}};

static const Key g_AVIDMJPEGECLabel = 
{{0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x04, 0x03, 0x01, 0x02, 0x01, 0x00, 0x00}};




static int read_uint32_be(FILE* fp, uint32_t* value)
{
    unsigned char buffer[4];
    if (fread(buffer, 1, 4, fp) != 4)
    {
        return 0;
    }
    
    *value = (buffer[0]<<24) + (buffer[1]<<16) + (buffer[2]<<8) + (buffer[3]);
    
    return 1;
}

static int compare_key(const Key* keyA, const Key* keyB, size_t cmpLen)
{
    return memcmp(keyA->_key, keyB->_key, cmpLen) == 0;
}

static int compare_key_masked(const Key* keyA, const Key* keyB, const Key* mask)
{
    int i = 0;
    for (i = 0; i < 16; i++)
    {
        if ((keyA->_key[i] & mask->_key[i]) != (keyB->_key[i] & mask->_key[i]))
        {
            return 0;
        }
    }
    return 1;
}

static void print_key(Key *p_key)
{
	int i;
	printf("K=");
	for (i = 0; i < 16; i++)
		printf("%02x ", p_key->_key[i]);
	printf("\n");
}

static int readKL(FILE *fp, Key *p_key, uint8_t* p_llen, uint64_t *p_len)
{
	int i;

	if (fread(p_key->_key, 16, 1, fp) != 1) 
    {
		if (feof(fp))
			return 0;

		perror("fread");
		fprintf(stderr, "Could not read Key\n");
		return 0;
	}

	// Read BER integer (ISO/IEC 8825-1).
	int c;
	uint64_t length = 0;
	if ((c = fgetc(fp)) == EOF) {
		perror("fgetc");
		fprintf(stderr, "Could not read Length\n");
				return 0;
	}

    *p_llen = 1;
	if (c < 128) {			// high-bit set on first byte?
		length = c;
	}
	else {				// else stores number of bytes in BER integer
		int bytes_to_read = c & 0x7f;
		for (i = 0; i < bytes_to_read; i++) {
			if ((c = fgetc(fp)) == EOF) {
				perror("fgetc");
				fprintf(stderr, "Could not read Length\n");
				return 0;
			}
			length = length << 8;
			length = length | c;
		}
        *p_llen += bytes_to_read;
	}
	*p_len = length;

	return 1;
}

static int read_label(FILE* fp, Key* label)
{
	if (fread(label->_key, 16, 1, fp) != 1) 
    {
		if (feof(fp))
			return 0;

		perror("fread");
		fprintf(stderr, "Could not read Label\n");
		return 0;
	}
    
    return 1;
}

static int read_batch_header(FILE* fp, uint32_t* len, uint32_t* eleLen)
{
    if (!read_uint32_be(fp, len) || !read_uint32_be(fp, eleLen))
    {
        return 0;
    }
    
    return 1;
}





int mxfe_get_essence_type(FILE* f, mxfe_EssenceType* type)
{
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to seek to start of file\n");
        return 0;
    }
    
    // locate header partition
    Key partPackKey;
    uint8_t partPackLLen;
    uint64_t partPackLen;    
    if (!readKL(f, &partPackKey, &partPackLLen, &partPackLen) || 
        !compare_key(&partPackKey, &g_headerPPKey, 16))
    {
        return 0;
    }
    uint64_t fpos;
    if ((fpos = ftell(f)) < 0)
    {
        perror("ftell");
        fprintf(stderr, "Failed to tell file position\n");
        return 0;
    }

    // skip uninteresting partition pack metadata    
    if (fseek(f, 64, SEEK_CUR) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to skip partition elements before OP label\n");
        return 0;
    }
    
    // check UL for operational pattern Atom
    Key opLabel;
    if (!read_label(f, &opLabel))
    {
        return MXFE_UNKNOWN;
    }
    if (!compare_key_masked(&opLabel, &g_opAtomLabel, &g_opAtomLabelMask))
    {
        printf("Operational pattern is not Atom\n");
        *type = MXFE_UNKNOWN;
        return 1;
    }

    // check essence container labels
    int isAAFKLV = 0;
    uint32_t batchLen;
    uint32_t batchEleLen;
    if (!read_batch_header(f, &batchLen, &batchEleLen))
    {
        fprintf(stderr, "Failed to read essence container labels batch header\n");
        return 0;
    }
    if (batchEleLen != 16)
    {
        fprintf(stderr, "Unexpected batch of labels element length (%d)\n", batchEleLen);
        return 0;
    }
    Key label;
    uint32_t i;
    for (i = 0; i < batchLen; i++)
    {
        if (!read_label(f, &label))
        {
            fprintf(stderr, "Failed to read essence container label\n");
            return 0;
        }
        if (compare_key(&label, &g_essenceContainerAAFLabel, 16))
        {
            isAAFKLV = 1;
            break;
        }
        else if (compare_key(&label, &g_BWFClipWrappedECLabel, 16))
        {
            *type = MXFE_WAVPCM;
            return 1;
        }
        else if (compare_key(&label, &g_DV50ClipWrappedECLabel, 16))
        {
            *type =  MXFE_DV;
            return 1;
        }
        else if (compare_key(&label, &g_AVIDMJPEGECLabel, 16))
        {
            *type =  MXFE_AVIDMJPEG;
            return 1;
        }
    }
    
    if (isAAFKLV)
    {
        // TODO: the AAF SDK does not yet do the right thing so we 
        // need to check the header for clues about the essence data type
        *type = MXFE_UNKNOWN;
        return 1;
    }
    
    *type = MXFE_UNKNOWN;
    return 1;
}

int mxfe_get_essence_element_info(FILE* f, uint64_t* offset, uint64_t* len)
{
    if (fseek(f, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to seek to start of file\n");
        return 0;
    }
    
    Key key;
    uint8_t tllen;
    uint64_t tlen;
    int foundBodyPartition = 0;
    while (1)
    {
        if (!readKL(f, &key, &tllen, &tlen))
        {
            fprintf(stderr, "Failed to read KL\n");
            return 0;
        }
        if (compare_key(&key, &g_bodyPPKey, 16))
        {
            foundBodyPartition = 1;
            break;
        }
        else
        {
            if (fseek(f, tlen, SEEK_CUR) != 0)
            {
                perror("fseek");
                fprintf(stderr, "Failed to skip value\n");
                return 0;
            }
        }
    }
    if (!foundBodyPartition)
    {
        fprintf(stderr, "No closed and complete body partition found\n");
        return 0;
    }

    // find the essence element key
    if (fseek(f, tlen, SEEK_CUR) != 0)
    {
        perror("fseek");
        fprintf(stderr, "Failed to skip body partition pack\n");
        return 0;
    }
    while (1)
    {
        if (!readKL(f, &key, &tllen, &tlen))
        {
            fprintf(stderr, "Failed to read KL\n");
            return 0;
        }
        else if (compare_key(&key, &g_essElementPrefixKey, 8))
        {
            if ((*offset = ftell(f)) < 0)
            {
                perror("ftell");
                fprintf(stderr, "Failed to tell file position\n");
                return 0;
            }
            *len = tlen;
            return 1;
        }
        
        else if (compare_key(&key, &g_ppPrefixKey, 13))
        {
            fprintf(stderr, "Unexpected end of body partition\n");
            return 0;
        }
        else
        {
            if (fseek(f, tlen, SEEK_CUR) != 0)
            {
                perror("fseek");
                fprintf(stderr, "Failed to skip value\n");
                return 0;
            }
        }
    }
    
    fprintf(stderr, "Failed to get essence data information\n");
    return 0;
}

int mxfe_get_essence_suffix(mxfe_EssenceType type, const char** suffix)
{
    assert(type < sizeof(essenceTypeSuffixes) / sizeof(struct {mxfe_EssenceType type; const char* suffix;}));
    
    *suffix = essenceTypeSuffixes[type].suffix;
    
    return 1;
}


