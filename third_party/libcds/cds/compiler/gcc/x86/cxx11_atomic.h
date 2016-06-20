/*
    This file is a part of libcds - Concurrent Data Structures library

    (C) Copyright Maxim Khizhinsky (libcds.dev@gmail.com) 2006-2016

    Source code repo: http://github.com/khizmax/libcds/
    Download: http://sourceforge.net/projects/libcds/files/
    
    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice, this
      list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
    AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
    CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
    OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.     
*/

#ifndef CDSLIB_COMPILER_GCC_X86_CXX11_ATOMIC_H
#define CDSLIB_COMPILER_GCC_X86_CXX11_ATOMIC_H

#include <cstdint>
#include <cds/compiler/gcc/x86/cxx11_atomic32.h>

//@cond
namespace cds { namespace cxx11_atomic {
    namespace platform { CDS_CXX11_INLINE_NAMESPACE namespace gcc { CDS_CXX11_INLINE_NAMESPACE namespace x86 {

        //-----------------------------------------------------------------------------
        // 64bit primitives
        //-----------------------------------------------------------------------------

        template <typename T>
        static inline bool cas64_strong( T volatile * pDest, T& expected, T desired, memory_order mo_success, memory_order mo_fail ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T) == 8, "Illegal size of operand" );
            assert( cds::details::is_aligned( pDest, 8 ));

            uint32_t ebxStore;
            T prev = expected;

            fence_before(mo_success);

            // We must save EBX in PIC mode
            __asm__ __volatile__ (
                "movl %%ebx, %[ebxStore]\n"
                "movl %[desiredLo], %%ebx\n"
                "lock; cmpxchg8b 0(%[pDest])\n"
                "movl %[ebxStore], %%ebx\n"
                : [prev] "=A" (prev), [ebxStore] "=m" (ebxStore)
                : [desiredLo] "D" ((int)desired), [desiredHi] "c" ((int)(desired >> 32)), [pDest] "S" (pDest), "0" (prev)
                : "memory");
            bool success = (prev == expected);
            if (success)
                fence_after(mo_success);
            else {
                fence_after(mo_fail);
                expected = prev;
            }
            return success;
        }

        template <typename T>
        static inline bool cas64_weak( T volatile * pDest, T& expected, T desired, memory_order mo_success, memory_order mo_fail ) CDS_NOEXCEPT
        {
            return cas64_strong( pDest, expected, desired, mo_success, mo_fail );
        }

        template <typename T>
        static inline T load64( T volatile const * pSrc, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T) == 8, "Illegal size of operand" );
            assert( order ==  memory_order_relaxed
                || order ==  memory_order_consume
                || order ==  memory_order_acquire
                || order ==  memory_order_seq_cst
                );
            assert( pSrc );
            assert( cds::details::is_aligned( pSrc, 8 ));
            CDS_UNUSED( order );

            T CDS_DATA_ALIGNMENT(8) v;
            __asm__ __volatile__(
                "movq   (%[pSrc]), %[v]   ;   \n\t"
                : [v] "=x" (v)
                : [pSrc] "r" (pSrc)
                :
            );
            return v;
        }


        template <typename T>
        static inline T exchange64( T volatile * pDest, T v, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T) == 8, "Illegal size of operand" );
            assert( cds::details::is_aligned( pDest, 8 ));

            T cur = load64( pDest, memory_order_relaxed );
            do {
            } while (!cas64_weak( pDest, cur, v, order, memory_order_relaxed ));
            return cur;
        }

        template <typename T>
        static inline void store64( T volatile * pDest, T val, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T) == 8, "Illegal size of operand" );
            assert( order ==  memory_order_relaxed
                || order ==  memory_order_release
                || order == memory_order_seq_cst
                );
            assert( pDest );
            assert( cds::details::is_aligned( pDest, 8 ));

            if ( order != memory_order_seq_cst ) {
                fence_before( order );
                // Atomically stores 64bit value by SSE instruction
                __asm__ __volatile__(
                    "movq       %[val], (%[pDest])   ;   \n\t"
                    :
                    : [val] "x" (val), [pDest] "r" (pDest)
                    : "memory"
                    );
            }
            else {
                exchange64( pDest, val, order );
            }
        }


        //-----------------------------------------------------------------------------
        // pointer primitives
        //-----------------------------------------------------------------------------

        template <typename T>
        static inline T * exchange_ptr( T * volatile * pDest, T * v, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T *) == sizeof(void *), "Illegal size of operand" );

            return (T *) exchange32( (uint32_t volatile *) pDest, (uint32_t) v, order );
        }

        template <typename T>
        static inline void store_ptr( T * volatile * pDest, T * src, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T *) == sizeof(void *), "Illegal size of operand" );
            assert( order ==  memory_order_relaxed
                || order ==  memory_order_release
                || order == memory_order_seq_cst
                );
            assert( pDest );

            if ( order != memory_order_seq_cst ) {
                fence_before( order );
                *pDest = src;
            }
            else {
                exchange_ptr( pDest, src, order );
            }
        }

        template <typename T>
        static inline T * load_ptr( T * volatile const * pSrc, memory_order order ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T *) == sizeof(void *), "Illegal size of operand" );
            assert( order ==  memory_order_relaxed
                || order ==  memory_order_consume
                || order ==  memory_order_acquire
                || order ==  memory_order_seq_cst
                );
            assert( pSrc );

            T * v = *pSrc;
            fence_after_load( order );
            return v;
        }

        template <typename T>
        static inline bool cas_ptr_strong( T * volatile * pDest, T *& expected, T * desired, memory_order mo_success, memory_order mo_fail ) CDS_NOEXCEPT
        {
            static_assert( sizeof(T *) == sizeof(void *), "Illegal size of operand" );

            return cas32_strong( (uint32_t volatile *) pDest, *reinterpret_cast<uint32_t *>( &expected ), (uint32_t) desired, mo_success, mo_fail );
        }

        template <typename T>
        static inline bool cas_ptr_weak( T * volatile * pDest, T *& expected, T * desired, memory_order mo_success, memory_order mo_fail ) CDS_NOEXCEPT
        {
            return cas_ptr_strong( pDest, expected, desired, mo_success, mo_fail );
        }
    }} // namespace gcc::x86

#ifndef CDS_CXX11_INLINE_NAMESPACE_SUPPORT
        using namespace gcc::x86;
#endif
    }   // namespace platform
}}  // namespace cds::cxx11_atomic
//@endcond

#endif // #ifndef CDSLIB_COMPILER_GCC_X86_CXX11_ATOMIC_H
