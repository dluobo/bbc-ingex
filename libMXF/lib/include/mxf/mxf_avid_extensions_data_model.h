/*
 * $Id: mxf_avid_extensions_data_model.h,v 1.3 2007/09/11 13:24:53 stuart_hc Exp $
 *
 * Avid data model extension definitions
 *
 * Copyright (C) 2006  Philip de Nier <philipn@users.sourceforge.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */
 

 
/* Note: defines are undefined at the end of the file */


#if !defined (MXF_BASIC_TYPE_DEF)
#define MXF_BASIC_TYPE_DEF(typeId, name, size)
#endif
#if !defined (MXF_ARRAY_TYPE_DEF)
#define MXF_ARRAY_TYPE_DEF(typeId, name, elementTypeId, fixedSize)
#endif
#if !defined (MXF_COMPOUND_TYPE_DEF)
#define MXF_COMPOUND_TYPE_DEF(typeId, name)
#endif
#if !defined (MXF_COMPOUND_TYPE_MEMBER)
#define MXF_COMPOUND_TYPE_MEMBER(memberName, memberTypeId)
#endif
#if !defined (MXF_INTERPRETED_TYPE_DEF)
#define MXF_INTERPRETED_TYPE_DEF(typeId, name, interpretedTypeId, fixedSize)
#endif

#if !defined (MXF_LABEL)
#define MXF_LABEL(d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, d10, d11, d12, d13, d14, d15)
#endif
#if !defined (MXF_SET_DEFINITION)
#define MXF_SET_DEFINITION(parentName, name, label)
#endif
#if !defined (MXF_ITEM_DEFINITION)
#define MXF_ITEM_DEFINITION(setName, name, label, localTag, typeId, isRequired)
#endif



MXF_ITEM_DEFINITION(GenericPictureEssenceDescriptor, ResolutionID,
    MXF_LABEL(0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb, 0xce, 0x2a, 0xca, 0x4d, 0x51, 0xab, 0x11, 0xd3),
    0x0000,
    MXF_INT32_TYPE,
    0
);
					
MXF_ITEM_DEFINITION(GenericPictureEssenceDescriptor, FrameSampleSize,
    MXF_LABEL(0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb, 0xce, 0x2a, 0xca, 0x50, 0x51, 0xab, 0x11, 0xd3),
    0x0000,
    MXF_INT32_TYPE,
    0
);
					
MXF_ITEM_DEFINITION(GenericPictureEssenceDescriptor, ImageSize,
    MXF_LABEL(0xa0, 0x24, 0x0, 0x60, 0x94, 0xeb, 0x75, 0xcb, 0xce, 0x2a, 0xca, 0x4f, 0x51, 0xab, 0x11, 0xd3),
    0x0000,
    MXF_INT32_TYPE,
    0
);
					

/* Note: this definition is incomplete. We only include it so that the 
   DataDefinition::Identification item can be read */
MXF_SET_DEFINITION(InterchangeObject, DefinitionObject, 
    MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1a, 0x00)
);
						
    MXF_ITEM_DEFINITION(DefinitionObject, Identification,
        MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x15, 0x03, 0x00, 0x00, 0x00, 0x00),
        0x1b02,
        MXF_UL_TYPE,
        1
    );

    
MXF_SET_DEFINITION(DefinitionObject, DataDefinition, 
    MXF_LABEL(0x06, 0x0e, 0x2B, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x1b, 0x00)
);
						

MXF_ITEM_DEFINITION(Preface, ProjectName,
    MXF_LABEL(0xa5, 0xfb, 0x7b, 0x25, 0xf6, 0x15, 0x94, 0xb9, 0x62, 0xfc, 0x37, 0x17, 0x49, 0x2d, 0x42, 0xbf),
    0x0000,
    MXF_UTF16STRING_TYPE,
    0
);

MXF_ITEM_DEFINITION(Preface, ProjectEditRate,
    MXF_LABEL(0x8c, 0x70, 0xa7, 0x18, 0x46, 0x7a, 0xe4, 0x86, 0xf3, 0x65, 0x46, 0xb1, 0x38, 0x7c, 0x4e, 0xe9),
    0x0000,
    MXF_RATIONAL_TYPE,
    0
);



MXF_ITEM_DEFINITION(GenericPackage, MobAttributeList,
    MXF_LABEL(0xa0, 0x1c, 0x00, 0x04, 0xac, 0x96, 0x9f, 0x50, 0x60, 0x95, 0x81, 0x83, 0x47, 0xb1, 0x11, 0xd4),
    0x0000,
    MXF_STRONGREFARRAY_TYPE,
    0
);

MXF_ITEM_DEFINITION(GenericPackage, UserComments,
    MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x03, 0x02, 0x01, 0x02, 0x0c, 0x00, 0x00, 0x00),
    0x4406,
    MXF_STRONGREFARRAY_TYPE,
    0
);

MXF_SET_DEFINITION(InterchangeObject, TaggedValue, 
    MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x3f, 0x00)
);

    MXF_ITEM_DEFINITION(TaggedValue, Name,
        MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x03, 0x02, 0x01, 0x02, 0x09, 0x01, 0x00, 0x00),
        0x5001,
        MXF_UTF16STRING_TYPE,
        1
    );

    MXF_ITEM_DEFINITION(TaggedValue, Value,
        MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0x02, 0x03, 0x02, 0x01, 0x02, 0x0a, 0x01, 0x00, 0x00),
        0x5003,
        MXF_INDIRECT_TYPE,
        1
    );

MXF_SET_DEFINITION(GenericDescriptor, TapeDescriptor, 
    MXF_LABEL(0x06, 0x0e, 0x2b, 0x34, 0x02, 0x53, 0x01, 0x01, 0x0d, 0x01, 0x01, 0x01, 0x01, 0x01, 0x2e, 0x00)
);



#undef MXF_BASIC_TYPE_DEF
#undef MXF_ARRAY_TYPE_DEF
#undef MXF_COMPOUND_TYPE_DEF
#undef MXF_COMPOUND_TYPE_MEMBER
#undef MXF_INTERPRETED_TYPE_DEF
#undef MXF_LABEL
#undef MXF_SET_DEFINITION
#undef MXF_ITEM_DEFINITION

