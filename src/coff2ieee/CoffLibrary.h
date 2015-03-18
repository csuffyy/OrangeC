/*
    Software License Agreement (BSD License)
    
    Copyright (c) 1997-2011, David Lindauer, (LADSoft).
    All rights reserved.
    
    Redistribution and use of this software in source and binary forms, 
    with or without modification, are permitted provided that the following 
    conditions are met:
    
    * Redistributions of source code must retain the above
      copyright notice, this list of conditions and the
      following disclaimer.
    
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the
      following disclaimer in the documentation and/or other
      materials provided with the distribution.
    
    * Neither the name of LADSoft nor the names of its
      contributors may be used to endorse or promote products
      derived from this software without specific prior
      written permission of LADSoft.
    
    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
    THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
    PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER 
    OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
    OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    contact information:
        email: TouchStone222@runbox.com <David Lindauer>
*/
#ifndef COFFLIBRARY_h
#define COFFLIBRARY_H

#include "CoffHeader.h"
#include "ObjFile.h"
#include "ObjIeee.h"
#include "ObjFactory.h"
#include "LibFromCoffDictionary.h"
#include "LibManager.h"
#include "LibFiles.h"


#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <set>
#include <map>

struct Module
{
    Module() : factory(&indexManager) { }
    std::set<std::string> aliases;
    ObjInt fileOffset;
    CoffLinkerMemberHeader header;
    bool import;
    bool ignore;
    ObjIeeeIndexManager indexManager;
    ObjFactory factory;
    ObjFile *file;
    std::string importName, importDLL;
};

class CoffLibrary
{
public:
    CoffLibrary(std::string Name) : name(Name), inputFile(NULL), importFile(NULL), importFactory(&importIndexManager) { }
    virtual ~CoffLibrary();

    bool Load();
    bool Convert();
    bool Write(std::string fileName);

protected:
    bool LoadNames();
    bool LoadHeaders();
    bool ScanIntegrity();
    bool ConvertNormalMods();
    bool ConvertImportMods();
    bool SaveLibrary(std::string fileName);
    void Align(FILE *ostr, ObjInt align = LibManager::ALIGN);
    
private:
    std::fstream *inputFile;        
    std::map<int, Module *> modules;
    std::string name;
    LibManager::LibHeader header;
    LibDictionary dictionary;
    LibFiles files;
    ObjIndexManager importIndexManager;
    ObjFactory importFactory;
    ObjFile *importFile;
};
#endif