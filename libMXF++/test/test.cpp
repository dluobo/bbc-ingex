/*
 * $Id: test.cpp,v 1.2 2009/04/16 17:52:50 john_f Exp $
 *
 * Test libMXF++
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
#include <memory>


using namespace std;
using namespace mxfpp;


static void testWrite()
{
    // open file
    auto_ptr<File> file(File::openNew("write_test.mxf"));
    file->setMinLLen(4);

    
    // header partition    
    Partition& headerPartition = file->createPartition();
    headerPartition.setKey(&MXF_PP_K(ClosedComplete, Header));
    headerPartition.setVersion(1, 2);
    headerPartition.setKagSize(0x100);
    headerPartition.setOperationalPattern(&MXF_OP_L(atom, complexity02));
    headerPartition.addEssenceContainer(&MXF_EC_L(DVBased_50_625_50_ClipWrapped));
    
    headerPartition.write(file.get());
    
    
    // header metadata
    auto_ptr<DataModel> dataModel(new DataModel());
    
    auto_ptr<AvidHeaderMetadata> headerMetadata(new AvidHeaderMetadata(dataModel.get()));
    
    Preface* preface = new Preface(headerMetadata.get());
    preface->setVersion(258);
    // etc.
    
    headerMetadata->write(file.get(), &headerPartition, 0);

    
    // body partition
    Partition& bodyPartition = file->createPartition();
    bodyPartition.setKey(&MXF_PP_K(ClosedComplete, Body));
    
    bodyPartition.write(file.get());

    // essence data
    

    // footer partition
    Partition& footerPartition = file->createPartition();
    footerPartition.setKey(&MXF_PP_K(ClosedComplete, Footer));
    
    footerPartition.write(file.get());
    
    
    // RIP
    file->writeRIP();
    
    // update partitions
    file->updatePartitions();
}

static void testRead(string filename)
{
    mxfKey key;
    uint8_t llen;
    uint64_t len;
    
    auto_ptr<File> file(File::openRead(filename));
    auto_ptr<DataModel> dataModel(new DataModel());
    auto_ptr<AvidHeaderMetadata> headerMetadata(new AvidHeaderMetadata(dataModel.get()));
    
    
    // read header partition
    auto_ptr<Partition> headerPartition(Partition::findAndReadHeaderPartition(file.get()));
    if (headerPartition.get() == 0)
    {
        throw "Could not find header partition";
    }

    
    // read header metadata
    file->readNextNonFillerKL(&key, &llen, &len);
    if (HeaderMetadata::isHeaderMetadata(&key))
    {
        headerMetadata->read(file.get(), headerPartition.get(), &key, llen, len);
        printf("Read header metadata\n");
    }
    else
    {
        throw "Could not find header metadata in header partition";
    }
    
    Preface* preface = headerMetadata->getPreface();
    preface->getInstanceUID();
    if (preface->haveGenerationUID())
    {
        preface->getGenerationUID();
    }
    printf("Preface::Version = %u\n", preface->getVersion());
    printf("size Preface::Identifications = %d\n", (int)preface->getIdentifications().size());
    printf("size Preface::EssenceContainers = %d\n", (int)preface->getEssenceContainers().size());
    
    Identification* identification = *preface->getIdentifications().begin();
    printf("Identification::CompanyName = '%s'\n", identification->getCompanyName().c_str());
    
    ContentStorage* storage = preface->getContentStorage();
    if (storage->haveEssenceContainerData())
    {
        storage->getEssenceContainerData();
    }
    
    // read remaining KLVs
    printf("Reading remaining KLVs and eventually fail");
    while (1)
    {
        file->readNextNonFillerKL(&key, &llen, &len);
        printf(".");
        fflush(stdout);
        //mxf_print_key(&key);
        file->skip(len);
    }
        
}



int main(int argc, const char* argv[])
{
    try
    {
        printf("Testing writing...\n");
        testWrite();
        printf("Done testing writing\n");
    }
    catch (MXFException& ex)
    {
        fprintf(stderr, "\nFailed:\n%s\n", ex.getMessage().c_str());
        return 1;
    }
    catch (const char*& ex)
    {
        fprintf(stderr, "\nFailed:\n%s\n", ex);
        return 1;
    }

    if (argc == 2)
    {
        const char* filename = argv[1];
     
        try
        {
            printf("Testing reading...\n");
            testRead(filename);
            printf("Done testing reading\n");
        }
        catch (MXFException& ex)
        {
            fprintf(stderr, "\nFailed:\n%s\n", ex.getMessage().c_str());
            return 1;
        }
        catch (const char*& ex)
        {
            fprintf(stderr, "\nFailed:\n%s\n", ex);
            return 1;
        }
    }

    return 0;
}


