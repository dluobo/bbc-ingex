/*
 * $Id: SwdFile.cpp,v 1.2 2008/08/07 16:41:48 john_f Exp $
 *
 * Class to read ScriptWriter II files.
 *
 * Copyright (C) 2007  British Broadcasting Corporation.
 * All Rights Reserved.
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

// ScriptWriter files Reader developed by Harjeev Chugh
//04/08/04


#pragma warning(disable:4786) // // To suppress some <vector>-related warnings
#include "SwdFile.h"

#include <iostream>

SwdFile::SwdFile (void)
    { ;}    

void SwdFile::fatalError(char* routineName, char* message)
    {
    std::cerr << ": Fatal error";
      if (verboseFlag) {
        std::cerr << " in routine \"" << routineName << "\"";
      }
      std::cerr << ": " << message << std::endl;

      exit(EXIT_FAILURE);
    }

char* SwdFile:: baseName(char* fullName)
    {
      char* result;
    #if defined(OM_OS_WINDOWS)
      const int delimiter = '\\';
    #elif defined(OM_OS_MACOS)
      const int delimiter = ':';
    #else
      const int delimiter = '/';
    #endif
      result = strrchr(fullName, delimiter);
      if (result == 0) {
        result = fullName;
      } else if (strlen(result) == 0) {
        result = fullName;
      } else {
        result++;
      }

      return result;
    }

void SwdFile::error(char* routineName, char* message)
    {
    std::cerr << ": Error";
      if (verboseFlag) {
        std::cerr << " in routine \"" << routineName << "\"";
      }
      std::cerr << ": " << message << std::endl;

      errorCount = errorCount + 1;
    }
void SwdFile::warning(char* routineName, char* message)
    {
      std::cerr << ": Warning";
      if (verboseFlag) {
        std::cerr << " in routine \"" << routineName << "\"";
      }
      std::cerr << ": " << message << std::endl;
      warningCount = warningCount + 1;
    }


int SwdFile:: checkStatus(const char* fileName, DWORD resultCode)
    {
      if (FAILED(resultCode)) {
        return FALSE;
      } else {
        return TRUE;
      }
    }


    #if defined(OM_UNICODE_APIS)

void SwdFile::convert(wchar_t* wcName, size_t length, const char* name)
    {
      size_t status  = mbstowcs(wcName, name, length);
      if (status == (size_t)-1) {
        fatalError("convert", "Conversion failed.");
      }
    }

void SwdFile:: convert(char* cName, size_t length, const wchar_t* name)
    {
      size_t status  = wcstombs(cName, name, length);
      if (status == (size_t)-1) {
        fatalError("convert", "Conversion failed.");
      }
    }

    #else

    // For use when wchar_t and OMCharacter are incompatible.
    // e.g. when sizeof(wchar_t) != sizeof(OMCharacter)
void SwdFile::convert(char* cName, size_t length, const OMCharacter* name)
    {
      for (size_t i = 0; i < length; i++) {
        char ch = name[i]; // truncate
        cName[i] = ch;
        if (ch == 0) {
          break;
        }
      }
    }
    #endif

void SwdFile::convert(char* cName, size_t length, const char* name)
    {
      size_t sourceLength = strlen(name);
      if (sourceLength < length - 1) {
        strncpy(cName, name, length);
      } else {
        fatalError("convert", "Conversion failed.");
      }
    }

void SwdFile::convertName(char* cName, size_t length, OMCHAR* wideName, char** tag)
    {
      char name[256];
      convert(name, 256, wideName);
      unsigned int first = name[0];
      char* pName;

      if (first < 0x03) {
        *tag = "OLE"; // OLE-managed
        pName = &name[1];
      } else if (first == 0x03) {
        *tag = "parent"; // parent-owned
        pName = &name[1];
      } else if (first == 0x04) {
        *tag = "SS reserved"; // for exclusive use of Structured Storage
        pName = &name[1];
      } else if (first <= 0x1f) {
        *tag = "OLE & O/S reserved"; // reserved for OLE and O/S
        pName = &name[1];
      } else {
        *tag = "application";
        pName = &name[0];
      }
      // Insert check for non-printable characters here.
      strncpy(cName, pName, length);
    }


    void SwdFile::resetStatistics(void)
    { 
      verboseFlag = false;
      SeqList.clear();
      OrderList.clear();
      OrderedSequence.clear();

      totalStorages = 0;
      totalStreams = 0;

      totalPropertyBytes = 0;
      totalObjects = 0;
      totalProperties = 0;

      totalStreamBytes = 0;
      totalFileBytes = 0;

      warningCount = 0;
      errorCount = 0;
    }

    void  SwdFile::initializeCOM(void)
    {
    #if !defined(OM_USE_REFERENCE_SS)
      CoInitialize(0);
    #endif
    }

    void SwdFile::finalizeCOM(void)
    {
    #if !defined(OM_USE_REFERENCE_SS)
      CoUninitialize();
    #endif
    }


    // Interpret values 0x00 - 0x7f as ASCII characters.
    //
    char SwdFile::table[128] = 
    {
    '.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
    '.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
    '.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
    '.',  '.',  '.',  '.',  '.',  '.',  '.',  '.',
    ' ',  '!',  '"',  '#',  '$',  '%',  '&', '\'',
    '(',  ')',  '*',  '+',  ',',  '-',  '.',  '/',
    '0',  '1',  '2',  '3',  '4',  '5',  '6',  '7',
    '8',  '9',  ':',  ';',  '<',  '=',  '>',  '?',
    '@',  'A',  'B',  'C',  'D',  'E',  'F',  'G',
    'H',  'I',  'J',  'K',  'L',  'M',  'N',  'O',
    'P',  'Q',  'R',  'S',  'T',  'U',  'V',  'W',
    'X',  'Y',  'Z',  '[', '\\',  ']',  '^',  '_',
    '`',  'a',  'b',  'c',  'd',  'e',  'f',  'g',
    'h',  'i',  'j',  'k',  'l',  'm',  'n',  'o',
    'p',  'q',  'r',  's',  't',  'u',  'v',  'w',
    'x',  'y',  'z',  '{',  '|',  '}',  '~',  '.'
    };







    char SwdFile::map(int c)
    {
      char result;
      if (c < 0x80) 
      {
        result = table[c & 0x7f];
      } 
      else 
      {
        result = '.';
      }
      return result;
    }

// Reads a whole stream and store it in a string

    void SwdFile::ReadStream (IStream* stream, STATSTG* statstg, 
                              char* pathName, std::string &str)
    {
        char ch;
        HRESULT result;
        str.empty();
        unsigned long int bytesRead;

        unsigned long int byteCount = statstg->cbSize.LowPart;
        if (statstg->cbSize.HighPart != 0) 
        {
            warning("ReadStream", "Large streams not handled.");
            byteCount = (size_t)-1;
        }

        totalStreamBytes = totalStreamBytes + byteCount;

        for (unsigned long int c = 0; c < byteCount; c++) 
        { 
            result = stream->Read(&ch, 1, &bytesRead);
            if (!checkStatus(pathName, result)) 
            {
              fatalError("ReadStream", "IStream::Read() failed.");
            }
            if (bytesRead != 1)
            {
              fatalError("ReadStream", "IStream::Read() read wrong number of bytes.");
            }
            //int ca = (unsigned char)ch ;
            //str += map(ca);
            str += ch;

        }  
        str += '\0';

    }

/*
// gets the order sequence
    void SwdFile:: StoreOrder(string slt)
    {   
        const char section [] = "SECTION ";
        char str2[15];
        char str1[5];
        char numb[50][5];
        int ind=0, newS=0,lind=0, mind=0;

        while (slt[ind] != 'p') 
        {
            while (slt[ind] != ',' && slt[ind] != 'p') 
            {   // searches for ',' or 'p' in loop
                // if not gets the next order seguence number
                numb[newS][lind] = slt[ind];
                str1[mind] = numb[newS][lind];
                if (numb[newS][lind] != ' ')
                ind++; lind++; mind++ ;      
            }

            str1[mind] = '\0';

            for (int f = 0; f < 8 ; f++)
            {
                str2[f] = section[f];
            }
            for ( f =8; f <= ((mind) + 8); f++)
            {
                str2[f] = str1[f-8];
            }
            OrderList.push_back(str2); // stores each sequence in a vector of string
            mind = 0;
            numb[newS][lind]=' ';
            if (slt[ind] != 'p') 
            {
                newS++;
                ind++;
                lind=0;
            }
        }
    }
*/


// finds the ordermode of the file and then stores the order in a vector of string

    void SwdFile::FindOrder(IStream* stream, STATSTG* statstg, char* pathName)
    {  
       const char mystring [] = " Major"; 
       const char mstring [] = " Orders.";
       const char mkstring [] = " = '";
       //const char strng [] = "SCRIPT DATA";
       //const char mnstring [] = " Order = ";
       std::string svt;
       
       char cname [12];  
       //char smt [10];
       //char mst [25];
       std::string str;
       char cName[256];
       char* tag;
       std::string slt;

       totalStreams = totalStreams + 1; // Count this stream

       OMCHAR* name = statstg->pwcsName;
       convertName(cName, 256, name, &tag);

       for (int n =0; n<11; n++)
       {
           cname[n] = cName[n];
       }
          cname[11] = '\0' ;
       if (strncmp("SCRIPT DATA", cname, 11) == 0) // if the cname is ScripData 
                                           // only then read a stream 
       { 
           ReadStream (stream, statstg, pathName, str);
#if 1

           unsigned long start = str.find(" Order = ");
           start += 9;
           unsigned long end = str.find_first_of("\r\n \t", start);
           std::string order_type = "Orders.";
           order_type += str.substr(start, end-start);
           order_type += " = ";
           // e.g. order_type may now contain "Orders.RunningOrder = "
#if 1
           // force reading of shooting order
           //order_type = "Orders.ShootingOrder = ";
           // force reading of normal order
           order_type = "Orders.NormalOrder = ";
#endif
           start = str.find(order_type, end);
           start += order_type.size();
           end = str.find('\'', start+2);
           start++;
           // Orders.RunningOrder = '1,3,5,95,7,53,97,55,57,9,117,101,63'
           // start and end should now delimit the comma-separated list
           char * order_list = new char[end - start + 1];
           str.copy(order_list, end - start, start);
           order_list[end - start] = 0; // null termination

            OrderList.clear();
            const char * seps = ", ";
            char * token = strtok(order_list, seps);
            while(token != NULL)
            {
                std::string s = "SECTION ";
                s += token;
                OrderList.push_back(s);
                token = strtok(NULL, seps);
            }
            delete [] order_list;
#else
           int n = 0;
           for (unsigned long int a=0; a <= str.size() ; a++)
           {
                for (unsigned long int i=0; i< 9; i++)
                { 
                    smt[i] = str[i + a];
                } 
                if (strncmp(" Order = ", smt,9) == 0) // search for order mode of file
                {   
                    int f = 0; 
                    while ( str [i + a + f] != '.' )
                    {
           
                        skt += str[i + a + f];     // store ordermode in string
                        f++;
                    }
                    skt += '\0';

                    for (n =0; n< 8; n++)
                    {   svt.empty();
                        svt += mstring[n];
                    }

                    for (n =8; n < f+8; n++)
                    {
                        svt += skt[n-8];
                    }
                    for ( n = f+8; n < (f+8) + 4; n++)
                    {
                        svt += mkstring[n-(f+8)];
                    }

                    unsigned long int x;
                    for (unsigned long int b=0; b <= str.size() ; b++)
                    {
                        for ( x=0; x < n; x++)
                        {
                            mst[x] = str[x + b];  
                        }
                        mst[x] = '\0' ;
                        if (strcmp(svt.c_str(), mst) == 0)
                        {
                            string order;
                            int p=0; 
                            while ( str[x+b+p] != ' ')
                            
                            {
                                order += str[x+b+p];  // store order sequence in a string
                                p++;
                            }
                    
                            string blnk = "'..";
                            string::size_type pos = order.find (blnk,0);
                            order.erase (pos,order.size());
                            slt = order;
                            slt.append(1,'p');
                        }
                    }

                    StoreOrder(slt); // store order in a vector of string


                }//if
           } //for
#endif
       }
         
     }


//-------------------------------------------------------------------------
// searches fot the data and then stores it in a vector of string

    void SwdFile ::StoreData(IStream* stream, STATSTG* statstg, char* pathName)
    { 
        unsigned long int i = 0;
        //const char mystring [] = " Major"; 
        //char st [7];
        std::string nam;
        const char strng [] = "SCRIPT DATA";
        //const char section[] = "SECTION";
        std::string str;
        std::string sect;
        totalStreams = totalStreams + 1; // Count this stream
        OMCHAR* name = statstg->pwcsName;
        char cName[256];
        char* tag;
        std::string list;

        convertName(cName, 256, name, &tag);
        nam = cName;
        sect.empty();
        for(i=0; i<7; i++)
            sect += cName[i];

        // Does name of stream begin with "SECTION"?
        // If so, we read it.
        if (strncmp("SECTION", sect.c_str(),7) == 0)
        { 
            //std::string y = nam;
            ReadStream (stream, statstg, pathName, str); // Reads a stream
#if 1
            // e.g. Name = 'Opening Titles'
            // or   Name = LOCOMOTION
            unsigned long start, end, next_object;
            start = str.find("Object CSequence");
            if (start < str.max_size())
            {
                // We found a CSequence in this stream
                Seq seq;
                seq.section = nam;
                next_object = str.find("Object", start + 10); // don't search into next object

                // Look for "Major = " which is followed by sequence number
                start = str.find("Major = ", start);
                if(start < next_object)
                {
                    start += 8;
                    end = str.find_first_of("\r\n \t",start); // find white space or newline
                    seq.number = str.substr(start, end - start);
                }
                // Look for "Name = " which is followed by sequence title
                start = str.find("Name = ", start);
                if(start < next_object)
                {
                    start += 7;
                    if(str[start] == '\'')
                    {
                        ++start;
                        end = str.find('\'',start); // find closing single-quote
                    }
                    else
                    {
                        end = str.find_first_of("\r\n \t",start); // find white space or newline
                    }
                    seq.name = str.substr(start, end - start);
                }
                SeqList.push_back(seq); // stores the sequence in a vector of string
            }
            else
            {
                //Seq seq;
                //seq.section = nam;
                //seq.number = "0";
                //seq.name = "no CSequence";
                //SeqList.push_back(seq); // stores the sequence in a vector of string
            }
#else
                for (unsigned long int a=0; a <= str.size() ; a++)
                {
                    for ( i=0; i< 6; i++)
                    {
                        st[i] = str[i + a];
                    }
                    st[6] = '\0';
                    if (strcmp(" Major", st) == 0)  // searches for "Major"
                    {
                        char scene [100];          // if found then gets the title..
                        const char bl = '.';
                        const char c [] = "'..";
                        for (int q=0; q<100; q++) 
                        {

                            if ( str[i+a+q] != (bl|c[0]))
                                scene[q] = str[i+a+q];
                            if ( str[i+a+q] == bl)
                            scene[q] = str[i+a+q+2];
                            if ( str[i+a+q] == c[0])
                            scene[q] = ' ';
                        }  
                        string s = scene;
                        string blk = "    ";
                        s.replace(6,10,"Title");
                        string::size_type pos = s.find (blk,0);
                        if (pos < 90 )
                        {
                            s.erase (pos,s.size());
                        }
                        list.empty();
                        list = nam;
                        list += "  SEQ";  
                        list += s;
                
                        Seq seq;
                        seq.section = nam;
                        seq.name = list;
                        SeqList.push_back(seq); // stores the sequence in a vector of string
                    }

                }
#endif          

        }
   }

// reads the storage and stream object

   void SwdFile::ReadStorage(IStorage* storage,
                     STATSTG* statstg,
                     char* pathName,
                     int isRoot)
    {
      totalStorages = totalStorages + 1; // Count this storage

      OMCHAR* name = statstg->pwcsName;
      char cName[256];
      char myPathName[256];
      char* tag;

      // Compute my name.

      strcpy(myPathName, pathName);

      if (isRoot)
      {
        convertName(cName, 256, name, &tag); // To fill in tag
        strcpy(cName, "");
      } else 
      {
        convertName(cName, 256, name, &tag);
      }
      strcat(myPathName, cName);
      

      if (!isRoot) {
        strcat(myPathName, "/");
      }

      HRESULT result;
      size_t status;

      IEnumSTATSTG* enumerator;
      result = storage->EnumElements(0, NULL, 0, &enumerator);
      if (!checkStatus(pathName, result)) {
        fatalError("ReadStorage", "IStorage::EnumElements() failed.");
      }
  
      result = enumerator->Reset();
      if (!checkStatus(pathName, result)) {
        fatalError("ReadStorage", "IStorage::Reset() failed.");
      }
  
      do {
        STATSTG statstg;
        status = enumerator->Next(1, &statstg, NULL);
        if (status == S_OK) {
      
          switch (statstg.type) {

          case STGTY_STORAGE: // if type is Storage object
            IStorage* subStorage;
            result = storage->OpenStorage(
              statstg.pwcsName,
              NULL,
              STGM_SHARE_EXCLUSIVE | STGM_READ,
              NULL,
              0,
              &subStorage);
            if (!checkStatus(pathName, result)) {
              fatalError("ReadStorage", "IStorage::OpenStorage() failed.");
            }
            ReadStorage(subStorage, &statstg, myPathName, 0);
            subStorage->Release();
            subStorage = 0;
            break;

          case STGTY_STREAM:   // else if a stream object
            IStream* subStream;
            result = storage->OpenStream( // open stream
              statstg.pwcsName,
              NULL,
              STGM_SHARE_EXCLUSIVE | STGM_READ,
              0,
              &subStream);
            if (!checkStatus(pathName, result)) {
              fatalError("ReadStorage", "IStorage::OpenStream() failed.");
            }
            FindOrder(subStream, &statstg, myPathName); 
            StoreData(subStream, &statstg, myPathName);
            
            
         

            subStream->Release();
            subStream = 0;
        
            
            break;

          default:
            std::cout << "unknown ";
            break;
          }
      
          // Having called EnumElements()/Next() we are responsible for
          // freeing statstg.pwcsName.
          //
          CoTaskMemFree(statstg.pwcsName);
        }
      } while (status == S_OK);

      if (status != S_FALSE) {
        // Loop was terminated by an error
        if (!checkStatus(pathName, status)) {
          fatalError("ReadStorage", "IEnumSTATSTG:Next() failed.");
        }
      }
    }


// opens the storage object
    
    void SwdFile::openStorage(const char* fileName, IStorage** storage)
    {
      HRESULT result;
      OMCHAR wcFileName[256];
      convert(wcFileName, 256, fileName); // convert filename to wchar_t* type
  
      result = StgIsStorageFile(wcFileName); // indicates whether file contains Storage or not
                                             // if it does then returns S_OK  
      switch (result) {
    
      case STG_E_INVALIDNAME:
        std::cerr << ": Error: "
             << "\"" << fileName << "\" is not a valid file name."
             << std::endl;
        exit(EXIT_FAILURE);
        break;
    
      case STG_E_FILENOTFOUND:
        std::cerr <<": Error: "
             << "File \"" << fileName << "\" not found."
             << std::endl;
        exit(EXIT_FAILURE);
        break;
      case STG_E_PATHNOTFOUND:
        std::cerr <<": Error: "
             << "Path \"" << fileName << "\" not found."
             << std::endl;
        exit(EXIT_FAILURE);
        break;
    
      }
  
      if (!checkStatus(fileName, result)) {
        fatalError("openStorage", "StgIsStorageFile() failed.");
      }
  
      if (result != S_OK) {
        std::cout << "\"" << fileName << "\" is NOT a structured storage file." << std::endl;
        exit(EXIT_FAILURE);
      }
  
      result = StgOpenStorage(
        wcFileName,
        NULL,
        STGM_READ | STGM_SHARE_DENY_WRITE,
        NULL,
        0,
        storage);
      if (!checkStatus(fileName, result)) {
        fatalError("openStorage", "StgOpenStorage() failed.");
      }

    }



// compare the orderlist with stored data and put data in order
void SwdFile:: GetOrderedData(void)
{
    int v = 0,m = 0;
    std::string s1, s2;
#if 1
    for(v = 0; v < OrderList.size(); ++v)
    {
        s1 = OrderList[v];
        for(m = 0; m < SeqList.size(); ++m)
        {
            if(0 == OrderList[v].compare(SeqList[m].section))
            {
                OrderedSequence.push_back(SeqList[m]);
                break;
            }
        }
    }
#else
    while ( v < OrderList.size())
    {               
        if(0 == OrderList[v].compare(SeqList[m].section))
        {   
            OrderedSequence.push_back(SeqList[m]);
            /*// debug
            s1.erase();
            s1 += "OrderList \"";
            s1 += OrderList[v];
            s1 += "\"";
            s2.erase();
            s2 += "OrderedSequence \"";
            s2 += OrderedSequence.back().section;
            s2 += "\", \"";
            s2 += OrderedSequence.back().name; */

            m++; v++;
            if (m == SeqList.size())
            {
                m = 0;
            }
        } 
        else
        {
            m++;
            if (m == SeqList.size() )
            {
            m = 0;
            }
        }   
    }
#endif

    /*// debug
    string s;
    for(v=0; v < OrderList.size(); ++v)
    {
        s = OrderList[v];
    }
    for(v=0; v < OrderedSequence.size(); ++v)
    {
        s = OrderedSequence[v].name;
    }*/
}

// Reads the file

    void SwdFile::SsfFile(const char* fileName)
    {
      resetStatistics();

      initializeCOM();
      IStorage* storage = 0;
      openStorage(fileName, &storage);

      if (storage == 0) {
        fatalError("SsfFile", "openStorage() failed.");
      }

      STATSTG statstg;
      HRESULT result = storage->Stat(&statstg, STATFLAG_DEFAULT);
      if (!checkStatus(fileName, result)) {
        fatalError("SsfFile", "IStorage::Stat() failed.");
      }
 
      ReadStorage(storage, &statstg, "/", 1);
 
  
  
      // Having called Stat() specifying STATFLAG_DEFAULT
      // we are responsible for freeing statstg.pwcsName.
      //
      CoTaskMemFree(statstg.pwcsName);
    
      // Releasing the last reference to the root storage closes the file.
      storage->Release();
      storage = 0;

      GetOrderedData();

 
      finalizeCOM();

  
    }


/* The main function -- 

    int main(int argumentCount, char* argumentVector[])
    {
      SwdFile SFile;
      SFile.programName = SFile.baseName(argumentVector[0]);

      int fileCount = 0;
      int flagCount = 0;
      int i;

        for (i = flagCount + 1; i < argumentCount; i++) {
          SFile.SsfFile(argumentVector[i]);

        }
        unsigned long int count;
        cout << "Please enter the Sequence number." << endl;
        if (cin >> count)
        { 
            if (getchar() == '\n')
            {
                if (count <= SFile.OrderedSequence.size())
                {
                    cout << "Sequence " << count << endl;
                    cout << SFile.OrderedSequence[count].c_str() << endl;
                }
                else 
                    cout << "no such sequence exists." << endl;
            }
            else
                cout << "no such sequence exists." << endl;
        }
        else 
            cout << "no such sequence exists." << endl;

      int result;
      if ((SFile.warningCount != 0) && (SFile.errorCount != 0)){
        cerr << SFile.programName << ": Swd completed with "
             << SFile.errorCount << " errors and "
             << SFile.warningCount << " warnings."
             << endl;
        result = EXIT_FAILURE;
      } else if (SFile.errorCount != 0) {
        cerr << SFile.programName << ": Swd completed with "
             << SFile.errorCount << " errors."
             << endl;
        result = EXIT_FAILURE;
      } else if (SFile.warningCount != 0) {
        cerr << SFile.programName << ": Swd completed with "
             << SFile.warningCount << " warnings."
             << endl;
        result = 2;
      } else {
        result = EXIT_SUCCESS;
      }
      return result;
    }
*/