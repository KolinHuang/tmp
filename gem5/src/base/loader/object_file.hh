/*
 * Copyright (c) 2002-2004 The Regents of The University of Michigan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Nathan Binkert
 *          Steve Reinhardt
 */

#ifndef __OBJECT_FILE_HH__
#define __OBJECT_FILE_HH__

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.hh"
#include "base/types.hh"

class PortProxy;
class Process;
class ProcessParams;
class SymbolTable;

class ObjectFile
{
  public:

    enum Arch {
        UnknownArch,
        Alpha,
        SPARC64,
        SPARC32,
        Mips,
        X86_64,
        I386,
        Arm64,
        Arm,
        Thumb,
        Power,
        Riscv64,
        Riscv32
    };

    enum OpSys {
        UnknownOpSys,
        Tru64,
        Linux,
        Solaris,
        LinuxArmOABI,
        FreeBSD
    };

    static const Addr maxAddr = std::numeric_limits<Addr>::max();

  protected:
    const std::string filename;
    uint8_t *fileData;
    size_t len;
    Addr loadOffset=0;
    Addr loadMask=maxAddr;

    Arch arch;
    OpSys opSys;

    ObjectFile(const std::string &_filename, size_t _len, uint8_t *_data,
               Arch _arch, OpSys _opSys);

  public:
    virtual ~ObjectFile();

    virtual bool loadSegments(const PortProxy &mem_proxy);

    virtual bool loadAllSymbols(SymbolTable *symtab, Addr base = 0,
                                Addr offset=0, Addr mask=maxAddr) = 0;
    virtual bool loadGlobalSymbols(SymbolTable *symtab, Addr base = 0,
                                   Addr offset=0, Addr mask=maxAddr) = 0;
    virtual bool loadLocalSymbols(SymbolTable *symtab, Addr base=0,
                                  Addr offset=0, Addr mask=maxAddr) = 0;
    virtual bool loadWeakSymbols(SymbolTable *symtab, Addr base=0,
                                 Addr offset=0, Addr mask=maxAddr)
    { return false; }

    virtual ObjectFile *getInterpreter() const { return nullptr; }
    virtual bool relocatable() const { return false; }
    virtual Addr mapSize() const
    { panic("mapSize() should only be called on relocatable objects\n"); }
    virtual void updateBias(Addr bias_addr)
    { panic("updateBias() should only be called on relocatable objects\n"); }
    virtual Addr bias() const { return 0; }

    virtual bool hasTLS() { return false; }

    Arch  getArch()  const { return arch; }
    OpSys getOpSys() const { return opSys; }

    struct Segment
    {
        std::string name;
        Addr base;
        uint8_t *data;
        size_t size;
    };

  protected:
    Addr entry;

    std::vector<std::unique_ptr<Segment>> segments;

    void
    addSegment(std::string name, Addr base, uint8_t *data, size_t size)
    {
        Segment *seg = new Segment;
        seg->name = name;
        seg->base = base;
        seg->data = data;
        seg->size = size;
        segments.emplace_back(seg);
    }

    bool loadSegment(Segment *seg, const PortProxy &mem_proxy);

  public:
    Addr entryPoint() const { return entry; }

    Addr
    maxSegmentAddr() const
    {
        Addr max = 0;
        for (auto &seg: segments) {
            Addr end = seg->base + seg->size;
            if (end > max)
                max = end;
        }
        return max;
    }

    Addr
    minSegmentAddr() const
    {
        Addr min = maxAddr;
        for (auto &seg: segments)
            if (seg->base < min)
                min = seg->base;
        return min;
    }

    bool
    contains(Addr addr) const
    {
        for (auto &seg: segments) {
            Addr start = seg->base;
            Addr end = seg->base + seg->size;
            if (addr >= start && addr < end)
                return true;
        }
        return false;
    }

    /* This function allows you to override the base address where
     * a binary is going to be loaded or set it if the binary is just a
     * blob that doesn't include an object header.
     * @param a address to load the binary/text section at
     */
    void setLoadOffset(Addr val) { loadOffset = val; }
    void setLoadMask(Addr val) { loadMask = val; }

    /**
     * Each instance of a Loader subclass will have a chance to try to load
     * an object file when tryLoaders is called. If they can't because they
     * aren't compatible with it (wrong arch, wrong OS, etc), then they
     * silently fail by returning nullptr so other loaders can try.
     */
    class Loader
    {
      public:
        Loader();

        /* Loader instances are singletons. */
        Loader(const Loader &) = delete;
        void operator=(const Loader &) = delete;

        virtual ~Loader() {}

        /**
         * Each subclass needs to implement this method. If the loader is
         * compatible with the passed in object file, it should return the
         * created Process object corresponding to it. If not, it should fail
         * silently and return nullptr. If there's a non-compatibliity related
         * error like file IO errors, etc., those should fail non-silently
         * with a panic or fail as normal.
         */
        virtual Process *load(ProcessParams *params, ObjectFile *obj_file) = 0;
    };

    // Try all the Loader instance's "load" methods one by one until one is
    // successful. If none are, complain and fail.
    static Process *tryLoaders(ProcessParams *params, ObjectFile *obj_file);
};

static inline std::ostream &
operator << (std::ostream &os, const ObjectFile::Segment &seg)
{
    ccprintf(os, "%s: %#x %d", seg.name, seg.base, seg.size);
    return os;
}

ObjectFile *createObjectFile(const std::string &fname, bool raw = false);


#endif // __OBJECT_FILE_HH__
