/*
 * $Id: DataModel.cpp,v 1.2 2009/10/22 16:36:37 philipn Exp $
 *
 * 
 *
 * Copyright (C) 2008  Philip de Nier <philipn@users.sourceforge.net>
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
 
#include <libMXF++/MXF.h>

#include <mxf/mxf_avid.h>

using namespace std;
using namespace mxfpp;


ItemType::ItemType(::MXFItemType* cItemType)
: _cItemType(cItemType)
{}

ItemType::~ItemType()
{}

ItemDef::ItemDef(::MXFItemDef* cItemDef)
: _cItemDef(cItemDef)
{}

ItemDef::~ItemDef()
{}

SetDef::SetDef(::MXFSetDef* cSetDef)
: _cSetDef(cSetDef)
{}

SetDef::~SetDef()
{}

bool SetDef::findItemDef(const mxfKey* key, ItemDef** itemDef)
{
    ::MXFItemDef* cItemDef;
    if (mxf_find_item_def_in_set_def(key, _cSetDef, &cItemDef))
    {
        *itemDef = new ItemDef(cItemDef);
        return true;
    }
    return false;
}




DataModel::DataModel()
{
    MXFPP_CHECK(mxf_load_data_model(&_cDataModel));
    MXFPP_CHECK(mxf_finalise_data_model(_cDataModel));
}

DataModel::~DataModel()
{
    mxf_free_data_model(&_cDataModel);
}
    

void DataModel::finalise()
{
    MXFPP_CHECK(mxf_finalise_data_model(_cDataModel));
}

bool DataModel::check()
{
    return mxf_check_data_model(_cDataModel) != 0;
}

void DataModel::registerSetDef(string name, const mxfKey* parentKey, const mxfKey* key)
{
    MXFPP_CHECK(mxf_register_set_def(_cDataModel, name.c_str(), parentKey, key));
}

void DataModel::registerItemDef(string name, const mxfKey* setKey,  const mxfKey* key,
    mxfLocalTag tag, unsigned int typeId, bool isRequired)
{
    MXFPP_CHECK(mxf_register_item_def(_cDataModel, name.c_str(), setKey, key, tag, typeId, isRequired));
}

ItemType* DataModel::registerBasicType(string name, unsigned int typeId, unsigned int size)
{
    ::MXFItemType* cItemType = mxf_register_basic_type(_cDataModel, name.c_str(), typeId, size);
    MXFPP_CHECK(cItemType != 0);
    return new ItemType(cItemType);
}

ItemType* DataModel::registerArrayType(string name, unsigned int typeId, unsigned int elementTypeId, unsigned int fixedSize)
{
    ::MXFItemType* cItemType = mxf_register_array_type(_cDataModel, name.c_str(), typeId, elementTypeId, fixedSize);
    MXFPP_CHECK(cItemType != 0);
    return new ItemType(cItemType);
}

ItemType* DataModel::registerCompoundType(string name, unsigned int typeId)
{
    ::MXFItemType* cItemType = mxf_register_compound_type(_cDataModel, name.c_str(), typeId);
    MXFPP_CHECK(cItemType != 0);
    return new ItemType(cItemType);
}

void DataModel::registerCompoundTypeMember(ItemType* itemType, string memberName, unsigned int memberTypeId)
{
    MXFPP_CHECK(mxf_register_compound_type_member(itemType->getCItemType(), memberName.c_str(), memberTypeId));
}

ItemType* DataModel::registerInterpretType(string name, unsigned int typeId, unsigned int interpretedTypeId, unsigned int fixedArraySize)
{
    ::MXFItemType* cItemType = mxf_register_interpret_type(_cDataModel, name.c_str(), typeId, interpretedTypeId, fixedArraySize);
    MXFPP_CHECK(cItemType != 0);
    return new ItemType(cItemType);
}
    
bool DataModel::findSetDef(const mxfKey* key, SetDef** setDef)
{
    ::MXFSetDef* cSetDef;
    if (mxf_find_set_def(_cDataModel, key, &cSetDef))
    {
        *setDef = new SetDef(cSetDef);
        return true;
    }
    return false;
}

bool DataModel::findItemDef(const mxfKey* key, ItemDef** itemDef)
{
    ::MXFItemDef* cItemDef;
    if (mxf_find_item_def(_cDataModel, key, &cItemDef))
    {
        *itemDef = new ItemDef(cItemDef);
        return true;
    }
    return false;
}

ItemType* DataModel::getItemDefType(unsigned int typeId)
{
    ::MXFItemType* cItemType = mxf_get_item_def_type(_cDataModel, typeId);
    MXFPP_CHECK(cItemType != 0);
    return new ItemType(cItemType);
}

bool DataModel::isSubclassOf(const mxfKey* setKey, const mxfKey* parentSetKey)
{
    return mxf_is_subclass_of(_cDataModel, setKey, parentSetKey) != 0;
}

bool DataModel::isSubclassOf(const MetadataSet* set, const mxfKey* parentSetKey)
{
    return isSubclassOf(set->getKey(), parentSetKey);
}

