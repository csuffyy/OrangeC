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
#include "DotNetPELib.h"
namespace DotNetPELib
{
    void DataContainer::Add(Field *field)
    {
        field->SetContainer(this);
        fields.push_back(field);
    }
    size_t DataContainer::GetParentNamespace()
    {
        DataContainer *current = this->GetParent();
        while (current && typeid(current) != typeid(Namespace *))
            current = current->GetParent();
        if (current)
            return current->GetPEIndex();
        return 0;
    }
    size_t DataContainer::GetParentClass()
    {
        DataContainer *current = GetParent();
        if (current && typeid(current) == typeid(Class *))
        {
            return current->GetPEIndex();
        }
        return 0;
    }
    bool DataContainer::ILSrcDump(PELib &peLib)
    {
        for (std::list<Field *>::iterator it = fields.begin(); it != fields.end(); ++it)
            (*it)->ILSrcDump(peLib);
        for (std::list<CodeContainer *>::iterator it = methods.begin(); it != methods.end(); ++it)
            (*it)->ILSrcDump(peLib);
        for (std::list<DataContainer *>::iterator it = children.begin(); it != children.end(); ++it)
            (*it)->ILSrcDump(peLib);
        return true;
    }
    bool DataContainer::PEDump(PELib &peLib)
    {
        for (std::list<Field *>::iterator it = fields.begin(); it != fields.end(); ++it)
            (*it)->PEDump(peLib);
        for (std::list<CodeContainer *>::iterator it = methods.begin(); it != methods.end(); ++it)
            (*it)->PEDump(peLib);
        for (std::list<DataContainer *>::iterator it = children.begin(); it != children.end(); ++it)
            (*it)->PEDump(peLib);
    }
    void DataContainer::Compile(PELib &peLib)
    {
        for (std::list<CodeContainer *>::iterator it = methods.begin(); it != methods.end(); ++it)
            (*it)->Render(peLib);
        for (std::list<DataContainer *>::iterator it = children.begin(); it != children.end(); ++it)
            (*it)->Render(peLib);
    }
    void DataContainer::GetBaseTypes(int &types)
    {
        for (std::list<CodeContainer *>::iterator it = methods.begin(); it != methods.end(); ++it)
            (*it)->GetBaseTypes(types);
        for (std::list<DataContainer *>::iterator it = children.begin(); it != children.end(); ++it)
            (*it)->GetBaseTypes(types);
        if (typeid(this) == typeid(Enum *))
            types |= basetypeEnum;
        else if (typeid(this) != typeid(Namespace *))
            if (flags.flags & Qualifiers::Value)
                types |= basetypeValue;
            else
                types |= basetypeObject;
    }
}