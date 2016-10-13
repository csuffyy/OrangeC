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
#include "PEFile.h"
#include <stdio.h>
namespace DotNetPELib
{

    void MethodSignature::AddVarargParam(Param *param)
    {
        param->SetIndex(params.size() + varargParams.size());
        varargParams.push_back(param);
    }
    bool MethodSignature::ILSrcDump(PELib &peLib, bool names, bool asType, bool PInvoke)
    {
        // this usage of vararg is for C style varargs
        // occil uses C# style varags except in pinvoke and generates
        // the associated object array argument
        if ((flags & Vararg) && !(flags & Managed))
        {
            peLib.Out() << "vararg ";
        }
        if (flags & Instance)
        {
            peLib.Out() << "instance ";
        }
        if (returnType->GetBasicType() == Type::cls)
        {
            if (returnType->GetClass()->GetFlags().flags & Qualifiers::Value)
                peLib.Out() << "valuetype ";
            else
                peLib.Out() << "class ";
        }
        returnType->ILSrcDump(peLib);
        peLib.Out() << " ";
        if (asType)
        {
            peLib.Out() << " *(";
        }
        else if (fullName.size())
        {
            peLib.Out() << fullName << "(";
        }
        else if (name.size())
        {
            if (names)
                peLib.Out() << "'" << name << "'(";
            else
                peLib.Out() << Qualifiers::GetName(name, container) << "(";
        }
        else
        {
            peLib.Out() << "(";
        }
        for(std::list<Param *>::iterator it = params.begin(); it != params.end();)
        {
            if ((*it)->GetType()->GetBasicType() == Type::cls)
            {
                if ((*it)->GetType()->GetClass()->GetFlags().flags & Qualifiers::Value)
                    peLib.Out() << "valuetype ";
                else
                    peLib.Out() << "class ";
            }
            (*it)->GetType()->ILSrcDump(peLib);
            if (names)
                (*it)->ILSrcDump(peLib);
            ++it;
            if (it != params.end())
                peLib.Out() << ", ";
        }
        if (!PInvoke && (flags & Vararg))
        {
            if (!(flags & Managed))
            {
                peLib.Out() << ", ...";
                if (varargParams.size())
                {
                    peLib.Out() << ", ";
                    for(std::list<Param *>::iterator it = varargParams.begin(); it != varargParams.end();)
                    {
                        (*it)->GetType()->ILSrcDump(peLib);
                        ++it;
                        if (it != varargParams.end())
                            peLib.Out() << ", ";
                    }
                }
            }
        }
        peLib.Out() << ")";
        return true;
    }
    bool MethodSignature::PEDump(PELib &peLib, bool asType)
    {
        if (fullName.size() != 0)
        {
            char assemblyName[1024],namespaceName[1024],className[1024], functionName[1024];
            if (sscanf(fullName.c_str(), "[%[^]]]%[^.].%s::%s", assemblyName, namespaceName, className, functionName) == 4)
            {
                AssemblyDef *assembly = peLib.FindAssembly(assemblyName);
                if (!assembly)
                {
                    peLib.AllocateAssemblyDef(assemblyName, true);
                    assembly = peLib.FindAssembly(assemblyName);
                    assembly->PEDump(peLib);
                }
                size_t nmspc = peLib.PEOut().HashString(namespaceName);
                size_t cls = peLib.PEOut().HashString(className);
                ResolutionScope rs(ResolutionScope::AssemblyRef, assembly->GetPEIndex() );
                TableEntryBase *table = new TypeRefTableEntry(rs, cls, nmspc);
                peIndexCallSite = peLib.PEOut().AddTableEntry(table);
                size_t sz;
                unsigned char *sig = SignatureGenerator::MethodRefSig(this, sz);
                size_t methodSignature = peLib.PEOut().HashBlob(sig, sz);
                delete[] sig;
                if (functionName[0] == '\'')
                {
                    strcpy(functionName, functionName + 1);
                    functionName[strlen(functionName)-1] = 0;
                }
                size_t function = peLib.PEOut().HashString(functionName);
                MemberRefParent memberRef(MemberRefParent::TypeRef, peIndexCallSite);
                table = new MemberRefTableEntry(memberRef, function, methodSignature);
                peIndexCallSite = peLib.PEOut().AddTableEntry(table);
            }
        }
        else if (asType)
        {
            if (!peIndexType)
            {
                size_t sz;
                unsigned char *sig = SignatureGenerator::MethodRefSig(this, sz);
                size_t methodSignature = peLib.PEOut().HashBlob(sig, sz);
                delete[] sig;
                TableEntryBase *table = new StandaloneSigTableEntry(methodSignature);
                peIndexType = peLib.PEOut().AddTableEntry(table);            
            }
        }
        else if (!peIndexCallSite || (flags & Vararg))
        {
            size_t sz;
            size_t function = peLib.PEOut().HashString(name);
            size_t parent = container->GetParentClass();
            MemberRefParent memberRef(MemberRefParent::TypeRef, parent);
            unsigned char *sig = SignatureGenerator::MethodRefSig(this, sz);
            size_t methodSignature = peLib.PEOut().HashBlob(sig, sz);
            delete[] sig;
            TableEntryBase *table = new MemberRefTableEntry(memberRef, function, methodSignature);
            peIndexCallSite = peLib.PEOut().AddTableEntry(table);            
        }
    }
}