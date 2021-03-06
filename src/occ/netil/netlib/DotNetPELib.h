/*
    Software License Agreement (BSD License)

    Copyright (c) 2016, David Lindauer, (LADSoft).
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
#include <list>
#include <vector>
#include <map>
#include <string>
#include <iostream>
#include <fstream>
#include <stdexcept>

// this is the main library header
// it allows creation of the methods and data that would be dumped into 
// a .net assembly.  The data can be either directly written to an 
// executable file or DLL, or dumped into a .IL file suitable for 
// viewing or assembling with the ilasm assembler.
//
//
//   The general usage of this library is to first create a PELib object,
//   then populate it with objects related to your program.   If you want you can
//   use members of the Allocator class to instantiate the various objects,
//   if you do this they will be deleted when the PELib object is deleted.   Otherwise
//   if you use operator new you are responsible for your own memory management.
//
//	 Once you have created a PELib object, it will create an AssemblyRef object
//   Which is the container for your program.   If you want to write a free standing program
//   you would put your methods and fields into this object.   However if you want to
//   write a DLL that .net assemblies can access, you need to create a namespace in the assemblyref
//   and at least one class in the namespace.   Methods and fields go into the inner class.
//
//   Another thing you have to do for .net compatibility is set the CIL flag
//   in the PELib object.   IF you have a free-standing program you might not 
//   want to set it, for example you can simplify code generation by putting
//   initialized data in the SDATA segment.   To put data in the SDATA segment
//   you add an initializer to a static field. (it makes the equivalent of a .DATA statement)

//   this library has full support for 32- bit pinvoke
//      you have to help it just a little in the case of pinvokes with varargs
//      by creating a method signature for each unique invocation of the pinvoke
//      that is independent of the main method for the pinvoke.
//      the main method does not have a vararg list but they unique invocations do
//      internally, the exe generator will store the main method in the MethodDef table
//      and the unique invocations in the MemberRef table.   An entry in the ImplMap
//      table will give information about what DLL the pinvoke is related to.
//
//   this library has only been tested with 32 bit programs, however it should
//   work with 64 bit programs as long as pinvoke isn't used.
//
//   Note that if you intend to pass managed methods to unmanaged code or vice
//      versa you may need to create transition thunks somewhere.   OCCIL has
//      a library that does this for you, assuming you only want to marshal
//      simple types.
//   OCCIL also has a library which does simple marshalling between C-style strings
//      and .net style strings.
//
//   This libray does simple peephole optimizations when an instruction
//   can be replaced with a shorter equivalent.   For example:
//			ldc.i4 # can sometimes be shortened into one of several optimized instructions
//			branches can sometimes be shortened
//			parameter and local variable indexes can be optimized
//				for local variables the library will also modify the local variable
//				list and put the most used local variables first (as a further optimization)
//          maximum stack is calculated
//
// there are various things missing:
//   Public and Private are the only scope modifiers currently
//	 no support for character set modifiers
//   
//   Custom Attributes cannot in general be defined however internally
//		we do handle the C# vararg one behind the scenes
//   Parameter handling is missing support for [in][out][opt] and default values
//	 if you want you can define your data statically in the SDATA, however,
//		you cannot write to such data if you enable CIL support in the corflags
//		CIL support is one of the requirements for being visible to .net assemblies
//		so generally you have to write either a Main function (tag it with an entry point)
//		or a .cctor special function for DLL initialization
//   References to other assemblies are currently only supported well enough
//      for the OCCIL compiler to get by.  They are generally handled by adding
//		a 'fullname' to a type or class or method and there are severe limitations as 
//      to what can be done with them.   A future version of this library
//      will remove the 'fullname' support and place objects related to references
//      in other assemblies into external assemblydefs related to those assemblies.
//	the 'string' type isn't fully supported yet.
//
namespace DotNetPELib
{

    // definitions for some common types
    typedef __int64 longlong;
    typedef unsigned __int64 ulonglong;
    typedef unsigned char Byte; /* 1 byte */
    typedef unsigned short Word; /* two bytes */
    typedef unsigned DWord; /* four bytes */

    // forward references
    class PELib;
    class Allocator;
    class Method;
    class Namespace;
    class Class;
    class Field;
    class Enum;
    class Instruction;
    class Value;
    class Local;
    class Param;
    class MethodSignature;
    class Operand;
    class Type;
    class FieldName;
    class DataContainer;
    class PEWriter;
    class PEMethod;

    /* contains data, types, methods */

    ///** Destructor base.
    // Used to support cleaning up objects when the allocator is used to create
    // them.  Every object that can be allocated eventualy inherits
    // this virtual destructor, so that the allocator can destruct them again later. 
    class DestructorBase
    {
    public:
        DestructorBase() { }
        DestructorBase(const DestructorBase &);
        virtual ~DestructorBase() { }
    };

    ///** errors can be thrown, generally when the IL code is being 'optimized'
    // but sometimes at other times
    class PELibError : public std::runtime_error
    {
    public:

        enum ErrorList
        {
            ///** More params are being aded after a vararg param has been added
            VarargParamsAlreadyDeclared,
            ///** If calculations show that the stack would underflow somewhere in the method
            StackUnderflow,
            ///** A label can be reached but the paths don't agree on how many
            // things are currently on the stack
            MismatchedStack,
            ///** The stack is unbalanced when the RET statement is hit
            // This can mean either that there are elements on the stack in
            // a function that has no return value, or there is not exactly '
            //one element on the stack when the function has a return value.
            StackNotEmpty,
            ///** A label has been defined twice
            DuplicateLabel,
            ///** A label that has been referenced was not found
            MissingLabel,
            ///** the short version of some branch was requested, but the target
            // is out of range.
            ShortBranchOutOfRange,
            ///** the short version of an instruction which references a
            // local variable or a parameter was referenced, but the index is out
            // of range
            IndexOutOfRange,
            ///** There are multiple entry points
            MultipleEntryPoints,
            ///** There is no entry point (note that if it is a DLL an entry point isn't needed)
            MissingEntryPoint
        };
        PELibError(ErrorList err, const std::string &Name = "") : errnum(err), std::runtime_error(std::string(errorNames[err]) + " " + Name)
        {
        }
        virtual ~PELibError() { }
        ErrorList Errnum() const { return errnum; }
    private:
        ErrorList errnum;
        static char *errorNames[];
    };
    // Qualifiers is a generic class that holds all the 'tags' you would see on various objects in
    // the assembly file.   Where possible things are handled implicitly for example 'nested'
    // will automatically be added when a class is nested in another class.
    class Qualifiers : public DestructorBase
    {
    public:
        enum {
            Public = 0x1,
            Private = 0x2,
            Static = 0x4,
            Explicit = 0x8,
            Ansi = 0x10,
            Sealed = 0x20,
            Enum = 0x40,
            Value = 0x80,
            Sequential = 0x100,
            Auto = 0x200,
            Literal = 0x400,
            HideBySig = 0x800,
            PreserveSig = 0x1000,
            SpecialName = 0x2000,
            RTSpecialName = 0x4000,
            CIL = 0x8000,
            Managed = 0x10000
        };
        enum
        {
            // settings appropriate for occil, e.g. everything is static.  add public/private...
            MainClass = Ansi | Sealed,
            ClassClass = Value | Sequential | Ansi | Sealed,
            ClassField = 0,
            FieldInitialized = Static,
            EnumClass = Enum | Auto | Ansi | Sealed,
            EnumField = Static | Literal | Public,
            PInvokeFunc = HideBySig | Static | PreserveSig,
            ManagedFunc = HideBySig | Static | CIL | Managed
        };
        Qualifiers::Qualifiers(int Flags) : flags_(Flags)
        {
        }
        Qualifiers::Qualifiers(const Qualifiers &old)
        {
            flags_ = old.flags_;
        }
        ///** most qualifiers come before the name of the item
        void ILSrcDumpBeforeFlags(PELib &) const;
        ///** but a couple of the method qualifiers come after the method definition
        void ILSrcDumpAfterFlags(PELib &) const;
        ///** get a name for a DataContainer object, suitable for use in an ASM file
        // The main problem is there is a separator character between the first class encountered
        // and its members, which is different depending on whether it is a type or a field
        static std::string GetName(std::string root, DataContainer *parent, bool type = false);
        int Flags() const { return flags_; }
    protected:
        static void ReverseNamePrefix(std::string &rv, DataContainer *parent, int &pos, bool type);
        static std::string GetNamePrefix(DataContainer *parent, bool type);
    private:
        static char * qualifierNames_[];
        static int afterFlags_;
        int flags_;
    };
    ///** base class that contains instructions/ labels
    // will be further overridden later to make a 'method'
    // definition
    class CodeContainer : public DestructorBase
    {
    public:
        CodeContainer(Qualifiers Flags) :flags_(Flags) { }
        ///** This is the interface to add a single CIL instruction
        void AddInstruction(Instruction *instruction);

        ///** it is possible to remove the last instruction
        Instruction *RemoveLastInstruction() {
            Instruction *rv = instructions_.back();
            instructions_.pop_back();
            return rv;
        }
        ///** Retrieve the last instruction
        Instruction *LastInstruction() const {
            return instructions_.back();
        }
        ///** Validate instructions
        void ValidateInstructions();
        ///** return flags member
        Qualifiers Flags() const { return flags_; }

        // some internal functions
        void BaseTypes(int &types) const;
        virtual void Optimize(PELib &);
        virtual bool ILSrcDump(PELib &) const;
        virtual bool PEDump(PELib &) { return false; }
        virtual void Render(PELib&) { }
        Byte *Compile(PELib &, size_t &sz);
        virtual void Compile(PELib&) { }
    protected:
        std::map<std::string, Instruction *> labels;
        void LoadLabels();
        void OptimizeLDC(PELib &);
        void OptimizeLDLOC(PELib &);
        void OptimizeLDARG(PELib &);
        void OptimizeBranch(PELib &);
        void CalculateOffsets();
        bool ModifyBranches();
        std::list<Instruction *> instructions_;
        Qualifiers flags_;
        DataContainer *parent_;
    };
    ///** base class that contains other datacontainers or codecontainers
    // that means it can contain namespaces, classes, methods, or fields
    // The main assemblyref which holds everything is one of these,
    // which means it acts as the 'unnamed' namespace.
    // when this class is overridden as something other than a namespace,
    // it cannot contain namespaces
    class DataContainer : public DestructorBase
    {
    public:
        ///** all classes have to extend from SOMETHING...  
        // this is enumerations for the ones we can create by default
        enum
        {
            ///**  reference to 'System::Object'
            basetypeObject = 1,
            ///**  reference to 'System::Value'
            basetypeValue = 2,
            ///**  reference to 'System::Enum'
            basetypeEnum = 4,
            ///** reference to 'System' namespace
            baseIndexSystem = 8
        };
        DataContainer(std::string Name, Qualifiers Flags) : name_(Name), flags_(Flags),
            parent_(nullptr), instantiated_(false), peIndex_(0)
        {
        }
        ///** Add another data container
        // This could be an assemblydef, namespace, class, or enumeration
        void Add(DataContainer *item)
        {
            item->parent_ = this;
            children_.push_back(item);
        }
        ///** Add a code container
        // This is always a Method definition
        void Add(CodeContainer *item)
        {
            methods_.push_back(item);
        }
        ///** Add a field
        void Add(Field *field);
        ///** A flag an app can use to tell if the class has been instantiated,
        // for example it might be used after a forward reference is resolved.
        // it is not used internally to the library
        bool IsInstantiated() const { return instantiated_; }
        void SetInstantiated() { instantiated_ = true; }
        ///** The immediate parent
        DataContainer *Parent() const { return parent_; }
        ///** The inner namespace parent
        size_t ParentNamespace() const;
        ///** The closest parent class
        size_t ParentClass() const;
        ///** The name
        std::string Name() const { return name_; }
        ///** The qualifiers
        Qualifiers Flags() const { return flags_; }
        ///** metatable index in the PE file for this data container
        size_t PEIndex() const { return peIndex_; }

        // internal functions
        virtual bool ILSrcDump(PELib &) const;
        virtual bool PEDump(PELib &);
        virtual void Compile(PELib&);
        void Number(int &n);
        // sometimes we want to traverse upwards in the tree
        void Render(PELib&);
        void BaseTypes(int &types) const;
    protected:
        std::list<DataContainer *> children_;
        std::list<CodeContainer *> methods_;
        std::list<Field *> fields_;
        DataContainer *parent_;
        Qualifiers flags_;
        std::string name_;
        bool instantiated_;
        size_t peIndex_; // generic index into a table or stream
    };
    ///** base class for assembly definitions
    // this holds the main assembly ( as a non-external assembly)
    // or can hold an external assembly
    class AssemblyDef : public DataContainer
    {
    public:
        AssemblyDef(std::string Name, bool External, Byte * KeyToken = 0) : DataContainer("", 0), name_(Name), external_(External), peIndex_(0)
        {
            if (KeyToken)
                memcpy(publicKeyToken_, KeyToken, 8);
            else
                memset(publicKeyToken_, 0, 8);
        }
        ///** Assembly name
        const std::string& Name() const { return name_; }
        ///** metatable index for the assembly in the PE file
        size_t PEIndex() const { return peIndex_; }
        virtual ~AssemblyDef() { }
        bool ILHeaderDump(PELib &);
        bool PEHeaderDump(PELib &);
    private:
        std::string name_;
        bool external_;
        size_t peIndex_;
        Byte publicKeyToken_[8];
    };
    ///** a namespace
    class Namespace : public DataContainer
    {
    public:
        Namespace::Namespace(std::string Name) : DataContainer(Name, Qualifiers(0))
        {
        }
        ///** Get the full namespace name including all parents
        std::string ReverseName(DataContainer *child);
        virtual bool ILSrcDump(PELib &) const override;
        virtual bool PEDump(PELib &) override;
    };

    /* a class, note that it cannot contain namespaces which is enforced at compile time*/
    /* note that all classes have to eventually derive from one of the System base classes
     * but that is handled internally */
     /* enums derive from this */
    class Class : public DataContainer
    {
    public:
        Class::Class(std::string Name, Qualifiers Flags, int Pack, int Size) : DataContainer(Name, Flags),
            pack_(Pack), size_(Size)
        {
        }
        ///** set the structure packing
        void pack(int pk) { pack_ = pk; }
        ///** set the structure size
        void size(int sz) { size_ = sz; }
        virtual bool ILSrcDump(PELib &) const override;
        virtual bool PEDump(PELib &) override;
        void ILSrcDumpClassHeader(PELib &) const;

    protected:
        int TransferFlags() const;
        int pack_;
        int size_;
    };
    ///** A method with code
    // CIL instructions are added with the 'Add' member of code container
    class Method : public CodeContainer
    {
    public:
        ///** a call to either managed or unmanaged code
        enum InvokeMode { CIL, PInvoke };
        ///** linkage type for unmanaged call.
        enum InvokeType { Cdecl, Stdcall };
        Method(MethodSignature *Prototype, Qualifiers flags, bool entry = false);

        ///** Set Pinvoke DLL name
        void SetPInvoke(std::string name, InvokeType type = Stdcall)
        {
            invokeMode_ = PInvoke;
            pInvokeName_ = name;
            pInvokeType_ = type;
        }
        ///** Add a local variable
        void AddLocal(Local *local);

        ///** return the signature
        MethodSignature *Signature() const { return prototype_; }

        ///** Iterate through local variables
        typedef std::vector<Local *>::iterator iterator;
        iterator begin() { return varList_.begin(); }
        iterator end() { return varList_.end(); }
        size_t size() const { return varList_.size(); }

        // Internal functions
        virtual bool ILSrcDump(PELib &) const override;
        virtual bool PEDump(PELib &) override;
        virtual void Compile(PELib&) override;
        virtual void Optimize(PELib &) override;
    protected:
        void OptimizeLocals(PELib &);
        void CalculateMaxStack();
        void CalculateLive();
        MethodSignature *prototype_;
        std::vector<Local *> varList_;
        std::string pInvokeName_;
        InvokeMode invokeMode_;
        InvokeType pInvokeType_;
        int maxStack_;
        bool entryPoint_;
        PEMethod *rendering_;
    };

    ///** a field, could be either static or non-static
    class Field : public DestructorBase
    {
    public:
        ///** Size for enumerated values
        enum ValueSize { i8, i16, i32, i64 };
        ///** Mode for the initialized value
        enum ValueMode {
            ///** No initialized value
            None,
            ///** Enumerated value, goes into the constant table
            Enum,
            ///** Byte stream, goes into the sdata
            Bytes
        };
        Field::Field(std::string Name, Type *tp, Qualifiers Flags) : mode_(Field::None), name_(Name), flags_(Flags),
            type_(tp), enumValue_(0), byteValue_(nullptr), byteLength_(0), ref_(0), peIndex_(0)
        {
        }
        ///** Add an enumeration constant
        // Note that the field does need to be part of an enumeration
        void AddEnumValue(longlong Value, ValueSize Size);
        ///** Add an SDATA initializer
        void AddInitializer(Byte *bytes, int len); // this will be readonly in ILONLY assemblies
        ///** Field Name
        const std::string &Name() const { return name_; }
        ///** Set the field's container
        void SetContainer(DataContainer *Parent) { parent_ = Parent; }
        DataContainer *GetContainer() const { return parent_; }
        ///** Fullname.  Primitive attempt at refernces to other .net assemblies
        void FullName(std::string FullName) { fullName_ = FullName; Ref(true); }
        const std::string &FullName() const { return fullName_; }
        ///** Type of field
        Type *FieldType() const { return type_; }
        void FieldType(Type *tp) { type_ = tp; }
        ///** Field qualifiers
        const Qualifiers &Flags() const { return flags_; }
        //* Is field referenced
        void Ref(bool Ref) { ref_ = Ref; }
        bool IsRef() const { return ref_; }
        ///** Index in the fielddef table
        size_t PEIndex() const { return peIndex_; }

        // internal functions
        static bool ILSrcDumpTypeName(PELib &peLib, ValueSize size);
        virtual bool ILSrcDump(PELib &) const;
        virtual bool PEDump(PELib &);
    protected:
        DataContainer *parent_;
        std::string name_;
        std::string fullName_;
        Qualifiers flags_;
        ValueMode mode_;
        Type *type_;
        union {
            longlong enumValue_;
            Byte *byteValue_;
        };
        int byteLength_;
        ValueSize size_;
        size_t peIndex_;
        bool ref_;
    };
    ///** A special kind of class: enum
    class Enum : public Class
    {
    public:
        Enum::Enum(std::string Name, Qualifiers Flags, Field::ValueSize Size) :
            size(Size), Class(Name, Flags, -1, -1)
        {
        }
        ///** Add an enumeration, give it a name and a value
        // This creates the Field definition for the enumerated value
        void AddValue(Allocator &allocator, std::string Name, longlong Value);

        // internal functions
        virtual bool ILSrcDump(PELib &) const override;
        virtual bool PEDump(PELib &) override;
    protected:
        Field::ValueSize size;
    };
    ///** the operand to an instruction 
    // this can contain a number, a string, or a reference to value
    // a value can be a field, methodsignature, local, or param reference
    //
    class Operand : public DestructorBase
    {
    public:
        enum OpSize { any, i8, u8, i16, u16, i32, u32, i64, u64, inative, r4, r8 };
        enum OpType { t_none, t_value, t_int, t_real, t_string, t_label };
        ///** Default constructor
        Operand::Operand() : type_(t_none) // no operand
        {
        }
        ///** Operand is a complex value
        Operand::Operand(Value *V) : type_(t_value), refValue_(V)
        {
        }
        ///** Operand is an integer constant
        Operand::Operand(longlong Value, OpSize Size) : type_(t_int), intValue_(Value), sz_(Size)
        {
        }
        Operand::Operand(int Value, OpSize Size) : Operand((longlong)Value, Size) { }
        Operand::Operand(unsigned Value, OpSize Size) : Operand((longlong)Value, Size) { }
        ///** Operand is a floating point constant
        Operand::Operand(double Value, OpSize Size) : type_(t_real), floatValue_(Value), sz_(Size)
        {
        }
        ///** Operand is a string
        Operand(std::string Value, bool) : type_(t_string) // string
        {
            stringValue_ = Value;
        }
        ///** Operand is a label
        Operand(std::string Value) : type_(t_label) // label
        {
            stringValue_ = Value;
        }
        ///** Get type of operand
        OpType OperandType() const { return type_; }
        ///** When operand is a complex value, return it
        Value * GetValue() const { return type_ == t_value ? refValue_ : nullptr; }
        ///** return the int value
        longlong IntValue() const { return intValue_; }
        ///** return the string value
        std::string StringValue() const { return stringValue_; }
        ///** return the float value
        double FloatValue() const { return floatValue_; }

        ///** Internal functions
        virtual bool ILSrcDump(PELib &) const;
        size_t Render(PELib &peLib, int opcode, int operandType, Byte *);
    protected:
        OpType type_;
        OpSize sz_;
        Value *refValue_;
        std::string stringValue_;
        longlong intValue_;
        double floatValue_;
        bool isnanorinf() const;
    };
    /* a CIL instruction */
    class Instruction : public DestructorBase
    {
    public:

        // names of opcodes
        enum iop
        {
            ///** should never occur
            i_unknown,
            ///** This instruction is a placeholder for a label
            i_label,
            ///** This instruction is a placeholder for a comment
            i_comment,
            ///** actual CIL instructions start here
            i_add, i_add_ovf, i_add_ovf_un, i_and, i_arglist, i_beq, i_beq_s, i_bge,
            i_bge_s, i_bge_un, i_bge_un_s, i_bgt, i_bgt_s, i_bgt_un, i_bgt_un_s, i_ble,
            i_ble_s, i_ble_un, i_ble_un_s, i_blt, i_blt_s, i_blt_un, i_blt_un_s, i_bne_un,
            i_bne_un_s, i_box, i_br, i_br_s, i_break, i_brfalse, i_brfalse_s, i_brinst,
            i_brinst_s, i_brnull, i_brnull_s, i_brtrue, i_brtrue_s, i_brzero, i_brzero_s, i_call,
            i_calli, i_callvirt, i_castclass, i_ceq, i_cgt, i_cgt_un, i_ckfinite, i_clt,
            i_clt_un, i_constrained_, i_conv_i, i_conv_i1, i_conv_i2, i_conv_i4, i_conv_i8, i_conv_ovf_i,
            i_conv_ovf_i_un, i_conv_ovf_i1, i_conv_ovf_i1_un, i_conv_ovf_i2, i_conv_ovf_i2_un, i_conv_ovf_i4, i_conv_ovf_i4_un, i_conv_ovf_i8,
            i_conv_ovf_i8_un, i_conv_ovf_u, i_conv_ovf_u_un, i_conv_ovf_u1, i_conv_ovf_u1_un, i_conv_ovf_u2, i_conv_ovf_u2_un, i_conv_ovf_u4,
            i_conv_ovf_u4_un, i_conv_ovf_u8, i_conv_ovf_u8_un, i_conv_r_un, i_conv_r4, i_conv_r8, i_conv_u, i_conv_u1,
            i_conv_u2, i_conv_u4, i_conv_u8, i_cpblk, i_cpobj, i_div, i_div_un, i_dup,
            i_endfault, i_endfilter, i_endfinally, i_initblk, i_initobj, i_isinst, i_jmp, i_ldarg,
            i_ldarg_0, i_ldarg_1, i_ldarg_2, i_ldarg_3, i_ldarg_s, i_ldarga, i_ldarga_s, i_ldc_i4,
            i_ldc_i4_0, i_ldc_i4_1, i_ldc_i4_2, i_ldc_i4_3, i_ldc_i4_4, i_ldc_i4_5, i_ldc_i4_6, i_ldc_i4_7,
            i_ldc_i4_8, i_ldc_i4_m1, i_ldc_i4_M1, i_ldc_i4_s, i_ldc_i8, i_ldc_r4, i_ldc_r8, i_ldelem,
            i_ldelem_i, i_ldelem_i1, i_ldelem_i2, i_ldelem_i4, i_ldelem_i8, i_ldelem_r4, i_ldelem_r8, i_ldelem_ref,
            i_ldelem_u1, i_ldelem_u2, i_ldelem_u4, i_ldelem_u8, i_ldelema, i_ldfld, i_ldflda, i_ldftn,
            i_ldind_i, i_ldind_i1, i_ldind_i2, i_ldind_i4, i_ldind_i8, i_ldind_r4, i_ldind_r8, i_ldind_ref,
            i_ldind_u1, i_ldind_u2, i_ldind_u4, i_ldind_u8, i_ldlen, i_ldloc, i_ldloc_0, i_ldloc_1,
            i_ldloc_2, i_ldloc_3, i_ldloc_s, i_ldloca, i_ldloca_s, i_ldnull, i_ldobj, i_ldsfld,
            i_ldsflda, i_ldstr, i_ldtoken, i_ldvirtftn, i_leave, i_leave_s, i_localloc, i_mkrefany,
            i_mul, i_mul_ovf, i_mul_ovf_un, i_neg, i_newarr, i_newobj, i_no_, i_nop,
            i_not, i_or, i_pop, i_readonly_, i_refanytype, i_refanyval, i_rem, i_rem_un,
            i_ret, i_rethrow, i_shl, i_shr, i_shr_un, i_sizeof, i_starg, i_starg_s,
            i_stelem, i_stelem_i, i_stelem_i1, i_stelem_i2, i_stelem_i4, i_stelem_i8, i_stelem_r4, i_stelem_r8,
            i_stelem_ref, i_stfld, i_stind_i, i_stind_i1, i_stind_i2, i_stind_i4, i_stind_i8, i_stind_r4,
            i_stind_r8, i_stind_ref, i_stloc, i_stloc_0, i_stloc_1, i_stloc_2, i_stloc_3, i_stloc_s,
            i_stobj, i_stsfld, i_sub, i_sub_ovf, i_sub_ovf_un, i_switch, i_tail_, i_throw,
            i_unaligned_, i_unbox, i_unbox_any, i_volatile_, i_xor
        };
        Instruction(iop Op, Operand *Operand);
        // for now only do comments and labels and branches...
        Instruction(iop Op, std::string Text) : op_(Op), text_(Text), switches_(nullptr), operand_(nullptr), live_(false) { }

        virtual ~Instruction() { if (switches_) delete switches_; }

        ///** Get the opcode
        iop OpCode() const { return op_; }
        ///** Set the opcode
        void OpCode(iop Op) { op_ = Op; }
        ///** Add a label for a SWITCH instruction
        ///** Labels MUST be added in order
        void AddCaseLabel(std::string label);
        ///** Get the set of case labels
        std::list<std::string> * GetSwitches() { return switches_; }
        ///** an 'empty' operand
        void NullOperand(Allocator &allocator);
        ///** Get the operand (CIL instructions have either zero or 1 operands)
        Operand *GetOperand() const { return operand_; }
        ///** Get text, e.g. for a comment
        std::string Text() const { return text_; }
        ///** Get the label name associated with the instruction
        std::string Label() const
        {
            if (operand_)
                return operand_->StringValue();
            else
                return "";
        }
        ///** The offset of the instruction within the method
        int Offset() const { return offset_; }
        ///** Set the offset of the instruction within the method
        void Offset(int Offset) { offset_ = Offset; }
        ///** Calculate length of instruction
        int InstructionSize();
        ///** get stack use for this instruction
        // positive means it adds to the stack, negative means it subtracts
        // 0 means it has no effect
        int StackUsage();
        ///** is a branch with a 4 byte relative offset
        int IsRel4() const { return instructions_[op_].operandType == o_rel4; }
        ///** is a branch with a 1 byte relative offset
        int IsRel1() const { return instructions_[op_].operandType == o_rel1; }
        ///** is any kind of branch
        int IsBranch() const { return IsRel1() || IsRel4(); }
        ///** Convert a 4-byte branch to a 1-byte branch
        void Rel4To1() { op_ = (iop)((int)op_ + 1); }
        ///** Set the live flag.   We are checking for live because sometimes
        // dead code sequences can confuse the stack checking routine
        void Live(bool val) { live_ = val; }
        ///** is it live?
        bool IsLive() const { return live_; }

        // internal methods and structures
        virtual bool ILSrcDump(PELib &) const;
        size_t Render(PELib & peLib, Byte *, std::map<std::string, Instruction *> &labels);
        enum ioperand {
            o_none, o_single, o_rel1, o_rel4, o_index1, o_index2, o_index4,
            o_immed1, o_immed4, o_immed8, o_float4, o_float8, o_switch
        };
        struct InstructionName {
            char *name;
            Byte op1;
            Byte op2;
            Byte bytes;
            Byte operandType;
            char stackUsage; // positive it adds to stack, negative it consumes stack
        };
    protected:
        std::list<std::string> *switches_;
        iop op_;
        int offset_;
        Operand *operand_; // for non-labels
        std::string text_; // for comments
        bool live_;
        static InstructionName instructions_[];
    };

    ///** a value, typically to be used as an operand
    // various other classes derive from this to make specific types of operand values
    class Value : public DestructorBase
    {
    public:
        Value(std::string Name, Type *tp) : name_(Name), type_(tp) { }
        ///** return type of value
        Type *GetType() const { return type_; }
        ///** return name
        const std::string &Name() const { return name_; }
        ///** set name
        void Name(const std::string name) { name_ = name; }

        ///** internal functions
        virtual bool ILSrcDump(PELib &) const;
        virtual size_t Render(PELib &peLib, int opcode, int OperandType, Byte *);
    protected:
        std::string name_;
        Type *type_;
    };
    // value = local variable
    class Local : public Value
    {
    public:
        Local(std::string Name, Type *Tp) : Value(Name, Tp), uses_(0), index_(-1) { }

        ///** return index of variable
        int Index() const { return index_; }

        // internal functions
        void IncrementUses() { uses_++; }
        int Uses() const { return uses_; }
        void Index(int Index) { index_ = Index; }
        virtual bool ILSrcDump(PELib &) const override;
        virtual size_t Render(PELib &peLib, int opcode, int OperandType, Byte *) override;
    private:
        int index_;
        int uses_;
    };
    // value: a parameter
    // noticably missing is support for [in][out][opt] and default values
    class Param : public Value
    {
    public:
        Param(std::string Name, Type *Tp) : Value(Name, Tp), index_(-1) { }

        ///** return index of argument
        void Index(int Index) { index_ = Index; }

        // internal functions
        int Index() const { return index_; }
        virtual bool ILSrcDump(PELib &) const override;
        virtual size_t Render(PELib &peLib, int opcode, int OperandType, Byte *) override;
    private:
        int index_;
    };
    // value: a field name (used as an operand)
    class FieldName : public Value
    {
    public:
        ///** constructor.  Can be used to make the field a reference to another
        // assembly, in a rudimentary way
        FieldName(Field *F, std::string Name = "", std::string Path = "") : field_(F), Value("", nullptr)
        {
            if (Name.size())
                if (Path.size())
                    field_->FullName(Path + "::'" + Name + "'");
                else
                    field_->FullName(std::string("'") + Name + "'");
        }
        ///** Get the field reference
        Field *GetField() const { return field_; }

        // Internal functions
        virtual bool ILSrcDump(PELib &) const override;
        virtual size_t Render(PELib &peLib, int opcode, int OperandType, Byte *) override;
    protected:
        Field *field_;
    };
    // value: a method name (used as an operand)
    class MethodName : public Value
    {
    public:
        ///** Constructor.   Can be used to set method->fullname, meaning it 
        // can reference other assemblies in a rudimentary way.   Such references
        // are always assumed to be to non-static member functions.
        MethodName(MethodSignature *M, std::string Name = "", std::string Path = "");
        MethodSignature *Signature() const { return signature_; }
        virtual bool ILSrcDump(PELib &) const override;
        virtual size_t Render(PELib &peLib, int opcode, int OperandType, Byte *) override;
    protected:
        MethodSignature *signature_;
    };
    ///** the signature for a method, has a return type and a list of params.
    // params can be either named or unnamed
    // if the signature is not managed it is PINVOKE
    // There are two types of vararg protocols supported.
    // When performing a PINVOKE, the native CIL vararg mechanism is used
    // because that is how things are marshalled.   But if varars are used
    // in the arguments to a managed function, the argument list will end with
    // an argument which is an szarray of objects.  It will be tagged appropriately
    // so that other .net assemblies e.g. C# programs know how to use it as a param
    // list, including the ability to specify an arbitrary number of params.
    // When these are passed about in a program you generate you may need to box
    // simple values to fit them in the array...
    class MethodSignature : public DestructorBase
    {
    public:
        enum { Vararg = 1, Managed = 2, Instance = 4 };
        MethodSignature(std::string Name, int Flags, DataContainer *Container) : container_(Container), name_(Name), flags_(Flags), returnType_(nullptr), ref_(false), peIndex_(0), peIndexCallSite_(0), peIndexType_(0), methodParent_(nullptr)
        {
        }
        ///** Get return type
        Type *ReturnType() const { return returnType_; }
        ///** Set return type
        void ReturnType(Type *type)
        {
            returnType_ = type;
        }
        //* * Add a parameter.  They need to be added in order.
        void AddParam(Param *param)
        {
            if (varargParams_.size())
            {
                throw PELibError(PELibError::VarargParamsAlreadyDeclared);
            }
            param->Index(params.size());
            params.push_back(param);
        }
        ///** Add a vararg parameter.   These are NATIVE vararg parameters not
        // C# ones.   They are only added to signatures at a call site...
        void AddVarargParam(Param *param);
        ///** This is the parent declaration for a call site signature with vararg
        // params (the methoddef version of the signature)
        void SignatureParent(MethodSignature *parent) { methodParent_ = parent; }
        ///** return the parent declaration for a call site signature with vararg
        // params (the methoddef version of the signature)
        MethodSignature *SignatureParent() { return methodParent_; }
        ///** Get name
        const std::string &Name() const { return name_; }
        ///** Set name
        void SetName(std::string Name) { name_ = Name; }
        ///** Get full name
        const std::string &FullName() { return fullName_; }
        ///** Set full name, rudimentary reference to other assembly
        void FullName(std::string FullName) { fullName_ = FullName; Ref(true); }
        // iterate through parameters
        typedef std::list<Param *>::iterator iterator;
        iterator begin() { return params.begin(); }
        iterator end() { return params.end(); }

        // iterate through vararg parameters
        typedef std::list<Param *>::iterator viterator;
        iterator vbegin() { return varargParams_.begin(); }
        iterator vend() { return varargParams_.end(); }
        ///** make it an instance member
        void SetInstanceFlag() { flags_ |= Instance; }
        ///** make it a vararg signature
        void SetVarargFlag() { flags_ |= Vararg; }

        ///** return qualifiers
        int Flags() const { return flags_; }
        ///** return parameter count
        size_t ParamCount() const { return params.size(); }
        ///** return vararg parameter count
        size_t VarargParamCount() const { return varargParams_.size(); }

        // various indexes into metadata tables
        size_t PEIndex() const { return peIndex_; }
        size_t PEIndexCallSite() const { return peIndexCallSite_; }
        size_t PEIndexType() const { return peIndexType_; }
        void SetPEIndex(size_t index) { peIndex_ = index; }

        // internal functions
        void Ref(bool Ref) { ref_ = Ref; }
        bool IsRef() const { return ref_; }
        virtual bool ILSrcDump(PELib &, bool names, bool asType, bool PInvoke) const;
        virtual bool PEDump(PELib &, bool asType);
    protected:
        MethodSignature *methodParent_;
        DataContainer *container_;
        Type *returnType_;
        std::string name_;
        std::string fullName_;
        int flags_;
        std::list<Param *> params, varargParams_;
        bool ref_;
        size_t peIndex_, peIndexCallSite_, peIndexType_;
    };
    ///** the type of a field or value
    class Type : public DestructorBase
    {
    public:
        enum BasicType {
            ///** type is a reference to a class
            cls,
            ///** type is a reference to a method signature
            method,
            /* below this is various CIL types*/
            Void, i8, u8, i16, u16, i32, u32, i64, u64, inative, unative, r32, r64, object, objectArray, string
        };
        Type(BasicType Tp, int PointerLevel) : tp_(Tp), pointerLevel_(PointerLevel), typeRef_(nullptr), methodRef_(nullptr), peIndex_(0)
        {
        }
        Type(DataContainer *clsref) : tp_(cls), pointerLevel_(0), typeRef_(clsref), methodRef_(nullptr), peIndex_(0)
        {
        }
        Type(MethodSignature *methodref) : tp_(method), pointerLevel_(0), typeRef_(nullptr),
            methodRef_(methodref), peIndex_(0)
        {
        }
        ///** Get the type of the Type object
        enum BasicType GetBasicType() const { return tp_; }
        ///** Set the type
        void SetBasicType(BasicType type) { tp_ = type; }
        ///** Get the class reference for class type objects
        DataContainer *GetClass() const { return typeRef_; }
        ///** Get the signature reference for method type objects
        MethodSignature *GetMethod() const { return methodRef_; }
        ///** Rudimentary support for referencing other assemblies
        void FullName(std::string name) { fullName_ = name; }
        ///** Rudimentary support for referencing other assemblies
        const std::string &FullName() const { return fullName_; }

        ///** Pointer indirection count
        void PointerLevel(int n) { pointerLevel_ = n; }
        ///** Pointer indirection count
        int PointerLevel() const { return pointerLevel_; }

        // internal functions
        virtual bool ILSrcDump(PELib &) const;
        virtual size_t Render(PELib &, Byte *);
        bool IsVoid() { return tp_ == Void && pointerLevel_ == 0; }
        size_t PEIndex() const { return peIndex_; }
    protected:
        std::string fullName_;
        int pointerLevel_;
        BasicType tp_;
        DataContainer *typeRef_;
        MethodSignature *methodRef_;
        size_t peIndex_;
    private:
        static char *typeNames_[];
    };
    ///** A boxed type, e.g. the reference to a System::* object which
    // represents the basic type
    class BoxedType : public Type
    {
    public:
        BoxedType(BasicType Tp) : Type(Tp, 0)
        {
        }

        ///** internal functions
        virtual bool ILSrcDump(PELib &) const override;
        virtual size_t Render(PELib &, Byte *) override;
    private:
        static char *typeNames_[];
    };

    ///** The allocator manages memory that various objects get constructed into
    // so that the objects can be cleanly deleted without the application having
    // to keep track of every object.
    class Allocator
    {
    public:
        Allocator() : first_(nullptr), current_(nullptr) { }
        virtual ~Allocator() { FreeAll(); }

        AssemblyDef *AllocateAssemblyDef(std::string Name, bool External, Byte *KeyToken = 0);
        Namespace *AllocateNamespace(std::string Name);
        Class *AllocateClass(std::string Name, Qualifiers Flags, int Pack, int Size);
        Method *AllocateMethod(MethodSignature *Prototype, Qualifiers flags, bool entry = false);
        Field *AllocateField(std::string Name, Type *tp, Qualifiers Flags);
        Enum *AllocateEnum(std::string Name, Qualifiers Flags, Field::ValueSize Size);
        Operand *AllocateOperand();
        Operand *AllocateOperand(Value *V);
        Operand *AllocateOperand(longlong Value, Operand::OpSize Size);
        Operand *AllocateOperand(int Value, Operand::OpSize Size) {
            return AllocateOperand((longlong)Value, Size);
        }
        Operand *AllocateOperand(unsigned Value, Operand::OpSize Size) {
            return AllocateOperand((longlong)Value, Size);
        }
        Operand *AllocateOperand(double Value, Operand::OpSize Size);
        Operand *AllocateOperand(std::string Value, bool);
        Operand *AllocateOperand(std::string Value);
        Instruction *AllocateInstruction(Instruction::iop Op, Operand *Operand = nullptr);
        Instruction *AllocateInstruction(Instruction::iop Op, std::string Text);
        Value *AllocateValue(std::string name, Type *tp);
        Local *AllocateLocal(std::string name, Type *tp);
        Param *AllocateParam(std::string name, Type *tp);
        FieldName *AllocateFieldName(Field *F, std::string Name = "", std::string Path = "");
        MethodName *AllocateMethodName(MethodSignature *M, std::string Name = "", std::string Path = "");
        MethodSignature *AllocateMethodSignature(std::string Name, int Flags, DataContainer *Container);
        Type *AllocateType(Type::BasicType Tp, int PointerLevel);
        Type *AllocateType(DataContainer *clsref);
        Type *AllocateType(MethodSignature *methodref);
        BoxedType *AllocateBoxedType(Type::BasicType Tp);
        Byte *AllocateBytes(size_t sz);
        enum
        {
            AllocationSize = 0x10000,
        };
    private:
        // heap block
        struct Block
        {
            Block() : next_(nullptr), offset_(0) { memset(bytes_, 0, AllocationSize); }
            Block*next_;
            int offset_;
            Byte bytes_[AllocationSize];
        };
        void *BaseAlloc(size_t size);
        void FreeBlock(Block *b);
        void FreeAll();

        Block *first_, *current_;
    };
    ///** this is the main class to instantiate
    // the constructor creates a working assembly, you put all your code and data into that
    class PELib : public Allocator
    {
    public:
        enum CorFlags {
            ///** Set this for compatibility with .net assembly imports,
            // unset it for standalone assemblies if you want to modify your
            // sdata
            ilonly = 1,
            ///** Set this if you want to force 32 bits - you will possibly need it
            // for pinvokes
            bits32 = 2
        };
        enum OutputMode { ilasm, peexe, pedll };
        ///** Constructor, creates a working assembly
        PELib(std::string AssemblyName, int CoreFlags);
        ///** Get the working assembly
        // This is the one with your code and data, that gets written to the output
        AssemblyDef *WorkingAssembly() const { return assemblyRefs_.front(); }
        ///** Add a reference to another assembly
        // At this point these assemblies don't contain anything, later you will
        // be able to populate them either directly or automatically (by having it read the
        // assembly file)
        void AddExternalAssembly(std::string assemblyName, Byte *publicKeyToken = nullptr);
        ///** Find an assembly
        AssemblyDef *FindAssembly(std::string assemblyName) const;
        ///** Pinvoke references are always added to this object
        void AddPInvokeReference(MethodSignature *methodsig, std::string dllname, bool iscdecl);
        // get the core flags
        int GetCorFlags() const { return corFlags_; }

        ///** write an output file, possibilities are a .il file, an EXE or a DLL
        // the file can also be tagged as either console or win32
        bool DumpOutputFile(std::string fileName, OutputMode mode, bool Gui);
        const std::string &FileName() const { return fileName_; }

        ///** internal declarations
        // creates the MSCorLib assembly
        AssemblyDef *MSCorLibAssembly(bool dump = false);

        virtual bool ILSrcDump(PELib &) { return ILSrcDumpHeader() && ILSrcDumpFile(); }
        std::fstream &Out() const { return *outputStream_; }
        PEWriter &PEOut() const { return *peWriter_; }
        std::map<size_t, size_t> moduleRefs;
    protected:
        bool ILSrcDumpHeader();
        bool ILSrcDumpFile();
        bool DumpPEFile(std::string name, bool isexe, bool isgui);
        std::list<AssemblyDef *>assemblyRefs_;
        std::list<Method *>pInvokeSignatures_;
        std::string assemblyName_;
        std::fstream *outputStream_;
        std::string fileName_;
        int corFlags_;
        PEWriter *peWriter_;
        static Byte defaultPublicKeyToken_[];
    };

} // namespace