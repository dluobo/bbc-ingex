/*
 * $Id: DataModel.h,v 1.1 2009/02/02 05:14:33 stuart_hc Exp $
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

#ifndef __MXFPP_DATA_MODEL_H__
#define __MXFPP_DATA_MODEL_H__



namespace mxfpp
{

    
class ItemType
{
public:
    ItemType(::MXFItemType* cItemType);
    ~ItemType();
    
    ::MXFItemType* getCItemType() const { return _cItemType; }
    
private:
    ::MXFItemType* _cItemType;
};
    
class ItemDef
{
public:
    ItemDef(::MXFItemDef* cItemDef);
    ~ItemDef();
    
    ::MXFItemDef* getCItemDef() const { return _cItemDef; }
    
private:
    ::MXFItemDef* _cItemDef;
};
    
class SetDef
{
public:
    SetDef(::MXFSetDef* cSetDef);
    ~SetDef();
    
    bool findItemDef(const mxfKey* key, ItemDef** itemDef);
    
    ::MXFSetDef* getCSetDef() const { return _cSetDef; }
    
private:
    ::MXFSetDef* _cSetDef;
};


class DataModel
{
public:
    DataModel();
    ~DataModel();
    
    void finalise();
    bool check();
    
    void registerSetDef(std::string name, const mxfKey* parentKey, const mxfKey* key);
    void registerItemDef(std::string name, const mxfKey* setKey,  const mxfKey* key,
        mxfLocalTag tag, unsigned int typeId, bool isRequired);
    
    ItemType* registerBasicType(std::string name, unsigned int typeId, unsigned int size);
    ItemType* registerArrayType(std::string name, unsigned int typeId, unsigned int elementTypeId, unsigned int fixedSize);
    ItemType* registerCompoundType(std::string name, unsigned int typeId);
    void registerCompoundTypeMember(ItemType* itemType, std::string memberName, unsigned int memberTypeId);
    ItemType* registerInterpretType(std::string name, unsigned int typeId, unsigned int interpretedTypeId, unsigned int fixedArraySize);
        
    bool findSetDef(const mxfKey* key, SetDef** setDef);
    bool findItemDef(const mxfKey* key, ItemDef** itemDef);
    
    ItemType* getItemDefType(unsigned int typeId);
    
    bool isSubclassOf(const mxfKey* setKey, const mxfKey* parentSetKey);

    
    ::MXFDataModel* getCDataModel() const { return _cDataModel; }
    
private:
    ::MXFDataModel* _cDataModel;
};



};






#endif


