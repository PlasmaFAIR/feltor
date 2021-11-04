#pragma once

#include <vector>
#include <omp.h>
#include "blas1_omp.h"
#include "densematrix_serial.h"

namespace dg{
namespace blas2{
namespace detail{
template<class T, unsigned NBFPE>
void doDenseSymv_omp(unsigned num_rows, unsigned num_cols, T alpha, const
        std::vector<const T*>& m_ptr, const T* RESTRICT x,
        T beta, T* RESTRICT y)
{
#pragma omp for nowait
    for( unsigned i=0; i<num_rows; i++)
    {
        T fpe [NBFPE] = {0};
        for( unsigned k=0; k<num_cols; k++)
        {
            T a = m_ptr[k][i];
            T b = x[ k];
            AccumulateFPE<T,NBFPE>( a,b, fpe);
        }
        // multiply fpe with alpha
        T fpe2 [NBFPE] = {0};
        for( unsigned k=0; k<NBFPE; k++)
            AccumulateFPE<T,NBFPE>( alpha, fpe[k], fpe2);
        // Finally add beta*y
        AccumulateFPE<T,NBFPE>( beta, y[i], fpe2);
        // Finally sum up everything starting with smallest value
        y[i] = 0;
        for( int k=(int)NBFPE-1; k>=0; k--)
            // round to nearest
            y[i] = y[i] + fpe2[k];
    }
}

template<class T, class Vector1>
void doDenseSymv(OmpTag, unsigned num_rows, unsigned num_cols, T alpha, const
        std::vector<const T*>& m_ptr, const Vector1& x,
        T beta, T* RESTRICT y)
{
    constexpr unsigned NBFPE = 2;
    const T* x_ptr = x.data();
    if(omp_in_parallel())
    {
        doDenseSymv_omp<T,NBFPE>( num_rows, num_cols, alpha, m_ptr, x_ptr, beta, y);
        return;
    }
    if(num_rows>dg::blas1::detail::MIN_SIZE)
    {
        #pragma omp parallel
        {
            doDenseSymv_omp<T,NBFPE>( num_rows, num_cols, alpha, m_ptr, x_ptr, beta, y);
        }
    }
    else
        doDenseSymv( SerialTag(), num_rows, num_cols, alpha, m_ptr, x_ptr, beta, y);

}

} //namespace detail
} //namespace blas2
} //namespace dg
