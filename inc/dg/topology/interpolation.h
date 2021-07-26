#pragma once
//#include <iomanip>

#include <cusp/coo_matrix.h>
#include <cusp/csr_matrix.h>
#include "grid.h"
#include "evaluation.h"
#include "functions.h"
#include "creation.h"
#include "operator_tensor.h"

/*! @file

  @brief 1D, 2D and 3D interpolation matrix creation functions
  */

namespace dg{
///@addtogroup typedefs
///@{
template<class real_type>
using IHMatrix_t = cusp::csr_matrix<int, real_type, cusp::host_memory>;
template<class real_type>
using IDMatrix_t = cusp::csr_matrix<int, real_type, cusp::device_memory>;
using IHMatrix = IHMatrix_t<double>;
using IDMatrix = IDMatrix_t<double>;
//typedef cusp::csr_matrix<int, double, cusp::host_memory> IHMatrix; //!< CSR host Matrix
//typedef cusp::csr_matrix<int, double, cusp::device_memory> IDMatrix; //!< CSR device Matrix
#ifndef MPI_VERSION
namespace x{
//introduce into namespace x
using IHMatrix = IHMatrix;
using IDMatrix = IDMatrix;
} //namespace x
#endif //MPI_VERSION

///@}

namespace create{
///@cond
namespace detail{

/**
 * @brief Evaluate n Legendre poloynomial on given abscissa
 *
 * @param xn normalized x-value on which to evaluate the polynomials: -1<=xn<=1
 * @param n  maximum order of the polynomial
 *
 * @return array of coefficients beginning with p_0(x_n) until p_{n-1}(x_n)
 */
template<class real_type>
std::vector<real_type> coefficients( real_type xn, unsigned n)
{
    assert( xn <= 1. && xn >= -1.);
    std::vector<real_type> px(n);
    if( xn == -1)
    {
        for( unsigned u=0; u<n; u++)
            px[u] = (u%2 == 0) ? +1. : -1.;
    }
    else if( xn == 1)
    {
        for( unsigned i=0; i<n; i++)
            px[i] = 1.;
    }
    else
    {
        px[0] = 1.;
        if( n > 1)
        {
            px[1] = xn;
            for( unsigned i=1; i<n-1; i++)
                px[i+1] = ((real_type)(2*i+1)*xn*px[i]-(real_type)i*px[i-1])/(real_type)(i+1);
        }
    }
    return px;
}

// evaluate n base polynomials for n given abscissas
template<class real_type>
std::vector<real_type> lagrange( real_type x, const std::vector<real_type>& xi)
{
    unsigned n = xi.size();
    std::vector<real_type> l( n , 1.);
    for( unsigned i=0; i<n; i++)
        for( unsigned k=0; k<n; k++)
        {
            if ( k != i)
                l[i] *= (x-xi[k])/(xi[i]-xi[k]);
        }
    return l;
}

template<class real_type>
std::vector<real_type> choose_1d_abscissas( real_type X, unsigned points_per_line, const RealGrid1d<real_type>& g, const thrust::host_vector<real_type>& abs, unsigned& col_begin)
{
    //determine which cell (X) lies in
    real_type xnn = (X-g.x0())/g.h();
    unsigned n = (unsigned)floor(xnn);
    //intervall correction
    if (n==g.N()) {
        n-=1;
    }
    // look for closest abscissa
    std::vector<real_type> xs( points_per_line, 0);
    // X <= *it
    auto it = std::lower_bound( abs.begin()+n*g.n(), abs.begin() + (n+1)*g.n(),
            X);
    col_begin = 0;
    switch( points_per_line)
    {
        case 1: xs[0] = 1.;
                if( it == abs.begin())
                    col_begin = 0;
                else if( it == abs.end())
                    col_begin = it - abs.begin() - 1;
                else
                {
                    if ( fabs(X - *it) < fabs( X - *(it-1)))
                        col_begin = it - abs.begin();
                    else
                        col_begin = it - abs.begin() -1;
                }
                break;
        case 2: if( it == abs.begin())
                {
                    xs[0] = *it;
                    xs[1] = *(it+1);
                    col_begin = 0;
                }
                else if( it == abs.end())
                {
                    xs[0] = *(it-2);
                    xs[0] = *(it-1);
                    col_begin = it - abs.begin() - 2;
                }
                else
                {
                    xs[0] = *(it-1);
                    xs[1] = *it;
                    col_begin = it - abs.begin() - 1;
                }
                break;
        case 4: if( it <= abs.begin() +1)
                {
                    it = abs.begin();
                    xs[0] = *it,     xs[1] = *(it+1);
                    xs[2] = *(it+2), xs[3] = *(it+3);
                    col_begin = 0;
                }
                else if( it >= abs.end() -2)
                {
                    it = abs.end();
                    xs[0] = *(it-4), xs[1] = *(it-3);
                    xs[2] = *(it-2), xs[3] = *(it-1);
                    col_begin = it - abs.begin() - 4;
                }
                else
                {
                    xs[0] = *(it-2), xs[1] = *(it-1);
                    xs[2] = *(it  ), xs[3] = *(it+1);
                    col_begin = it - abs.begin() - 2;
                }
                break;
    }
    return xs;
}

}//namespace detail
///@endcond
///@addtogroup interpolation
///@{
/*!@class hide_bcx_doc
 * @param bcx determines what to do when a point lies outside the boundary in x. If \c dg::PER, the point will be shifted topologically back onto the domain. Else the
 * point will be mirrored at the boundary: \c dg::NEU will then simply interpolate at the resulting point, \c dg::DIR will take the negative of the interpolation.
 (\c dg::DIR_NEU and \c dg::NEU_DIR apply \c dg::NEU / \c dg::DIR to the respective left or right boundary )
 * This means the result of the interpolation is as if the interpolated function were Fourier transformed with the correct boundary condition and thus extended beyond the grid boundaries.
 * Note that if a point lies directly on the boundary between two grid cells, the value of the polynomial to the right is taken.
*/
/*!@class hide_method
 * @param method Several interpolation methods are available: **dg** uses the native
 * dG interpolation scheme given by the grid, **nearest** searches for the
 * nearest point and copies its value, **linear** searches for the two (in 2d
 * four, etc.) closest points and linearly interpolates their values, **cubic**
 * searches for the four (in 2d 16, etc) closest points and interpolates a
 * cubic polynomial
 */

/**
 * @brief Create interpolation matrix
 *
 * The created matrix has \c g.size() columns and \c x.size() rows. Per default
 * it uses polynomial interpolation given by the dG polynomials, i.e. the
 * interpolation has order \c g.n() .
 * When applied to a vector the result contains the interpolated values at the
 * given interpolation points.  The given boundary conditions determine how
 * interpolation points outside the grid domain are treated.
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @param x X-coordinates of interpolation points
 * @param g The Grid on which to operate
 * @copydoc hide_bcx_doc
 * @copydoc hide_method
 *
 * @return interpolation matrix
 */
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation(
        const thrust::host_vector<real_type>& x,
        const RealGrid1d<real_type>& g,
        dg::bc bcx = dg::NEU,
        std::string method = "dg")
{
    if( method == "dg")
    {
        cusp::coo_matrix<int, real_type, cusp::host_memory> A( x.size(), g.size(), x.size()*g.n());

        int number = 0;
        dg::Operator<real_type> forward( g.dlt().forward());
        for( unsigned i=0; i<x.size(); i++)
        {
            real_type X = x[i];
            bool negative = false;
            g.shift( negative, X, bcx);

            //determine which cell (x) lies in
            real_type xnn = (X-g.x0())/g.h();
            unsigned n = (unsigned)floor(xnn);
            //determine normalized coordinates
            real_type xn = 2.*xnn - (real_type)(2*n+1);
            //intervall correction
            if (n==g.N()) {
                n-=1;
                xn = 1.;
            }
            //evaluate 2d Legendre polynomials at (xn, yn)...
            std::vector<real_type> px = detail::coefficients( xn, g.n());
            //...these are the matrix coefficients with which to multiply
            std::vector<real_type> pxF(px.size(),0);
            for( unsigned l=0; l<g.n(); l++)
                for( unsigned k=0; k<g.n(); k++)
                    pxF[l]+= px[k]*forward(k,l);
            unsigned col_begin = n*g.n();
            if( negative)
                for( unsigned l=0; l<g.n(); l++)
                    pxF[l]*=-1.;
            detail::add_line( A, number, i,  col_begin, pxF);
        }
        return A;
    }
    else
    {
        unsigned points_per_line = 1;
        if( method == "nearest")
            points_per_line = 1;
        else if( method == "linear")
            points_per_line = 2;
        else if( method == "cubic")
            points_per_line = 4;
        else
            throw std::runtime_error( "Interpolation method "+method+" not recognized!\n");
        cusp::coo_matrix<int, real_type, cusp::host_memory> A(
                x.size(), g.size(), x.size()*points_per_line);
        int number = 0;
        thrust::host_vector<real_type> abs = dg::create::abscissas( g);
        for( unsigned i=0; i<x.size(); i++)
        {
            real_type X = x[i];
            bool negative = false;
            g.shift( negative, X, bcx);

            unsigned col_begin = 0;
            std::vector<real_type> xs  = detail::choose_1d_abscissas( X,
                    points_per_line, g, abs, col_begin);

            std::vector<real_type> px = detail::lagrange( X, xs);
            if( negative)
                for( unsigned l=0; l<points_per_line; l++)
                    px[l]*=-1.;
            detail::add_line( A, number, i,  col_begin, px);
        }
        return A;
    }
}

/**
 * @brief Create interpolation matrix
 *
 * The created matrix has \c g.size() columns and \c x.size() rows. Per default
 * it uses polynomial interpolation given by the dG polynomials, i.e. the
 * interpolation has order \c g.n() .
 * When applied to a vector the result contains the interpolated values at the
 * given interpolation points.  The given boundary conditions determine how
 * interpolation points outside the grid domain are treated.
 * @snippet topology/interpolation_t.cu doxygen
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @param x X-coordinates of interpolation points
 * @param y Y-coordinates of interpolation points (\c y.size() must equal \c x.size())
 * @param g The Grid on which to operate
 * @copydoc hide_bcx_doc
 * @param bcy analogous to \c bcx, applies to y direction
 * @copydoc hide_method
 *
 * @return interpolation matrix
 */
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation(
        const thrust::host_vector<real_type>& x,
        const thrust::host_vector<real_type>& y,
        const aRealTopology2d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU,
        std::string method = "dg")
{
    assert( x.size() == y.size());
    cusp::array1d<real_type, cusp::host_memory> values;
    cusp::array1d<int, cusp::host_memory> row_indices;
    cusp::array1d<int, cusp::host_memory> column_indices;
    if( method == "dg")
    {
        std::vector<real_type> gauss_nodes = g.dlt().abscissas();
        dg::Operator<real_type> forward( g.dlt().forward());


        for( int i=0; i<(int)x.size(); i++)
        {
            real_type X = x[i], Y = y[i];
            bool negative=false;
            g.shift( negative,X,Y, bcx, bcy);

            //determine which cell (x,y) lies in
            real_type xnn = (X-g.x0())/g.hx();
            real_type ynn = (Y-g.y0())/g.hy();
            unsigned nn = (unsigned)floor(xnn);
            unsigned mm = (unsigned)floor(ynn);
            //determine normalized coordinates
            real_type xn =  2.*xnn - (real_type)(2*nn+1);
            real_type yn =  2.*ynn - (real_type)(2*mm+1);
            //interval correction
            if (nn==g.Nx()) {
                nn-=1;
                xn = 1.;
            }
            if (mm==g.Ny()) {
                mm-=1;
                yn =1.;
            }
            //Test if the point is a Gauss point since then no interpolation is needed
            int idxX =-1, idxY = -1;
            for( unsigned k=0; k<g.n(); k++)
            {
                if( fabs( xn - gauss_nodes[k]) < 1e-14)
                    idxX = nn*g.n() + k; //determine which grid column it is
                if( fabs( yn - gauss_nodes[k]) < 1e-14)
                    idxY = mm*g.n() + k;  //determine grid line
            }
            if( idxX < 0 && idxY < 0 ) //there is no corresponding point
            {
                //evaluate 2d Legendre polynomials at (xn, yn)...
                std::vector<real_type> px = detail::coefficients( xn, g.n()),
                                       py = detail::coefficients( yn, g.n());
                std::vector<real_type> pxF(g.n(),0), pyF(g.n(), 0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                    {
                        pxF[l]+= px[k]*forward(k,l);
                        pyF[l]+= py[k]*forward(k,l);
                    }
                std::vector<real_type> pxy( g.n()*g.n());
                //these are the matrix coefficients with which to multiply
                for(unsigned k=0; k<pyF.size(); k++)
                    for( unsigned l=0; l<pxF.size(); l++)
                        pxy[k*px.size()+l]= pyF[k]*pxF[l];
                for( unsigned k=0; k<g.n(); k++)
                    for( unsigned l=0; l<g.n(); l++)
                    {
                        row_indices.push_back( i);
                        column_indices.push_back( (mm*g.n()+k)*g.n()*g.Nx()+nn*g.n() + l);
                        if( !negative)
                            values.push_back(  pxy[k*g.n()+l]);
                        else
                            values.push_back( -pxy[k*g.n()+l]);
                    }
            }
            else if ( idxX < 0 && idxY >=0) //there is a corresponding line
            {
                std::vector<real_type> px = detail::coefficients( xn, g.n());
                std::vector<real_type> pxF(g.n(),0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                        pxF[l]+= px[k]*forward(k,l);
                for( unsigned l=0; l<g.n(); l++)
                {
                    row_indices.push_back( i);
                    column_indices.push_back( (idxY)*g.Nx()*g.n() + nn*g.n() + l);
                    if( !negative)
                        values.push_back( pxF[l]);
                    else
                        values.push_back(-pxF[l]);

                }
            }
            else if ( idxX >= 0 && idxY < 0) //there is a corresponding column
            {
                std::vector<real_type> py = detail::coefficients( yn, g.n());
                std::vector<real_type> pyF(g.n(),0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                        pyF[l]+= py[k]*forward(k,l);
                for( unsigned k=0; k<g.n(); k++)
                {
                    row_indices.push_back(i);
                    column_indices.push_back((mm*g.n()+k)*g.Nx()*g.n() + idxX);
                    if( !negative)
                        values.push_back( pyF[k]);
                    else
                        values.push_back(-pyF[k]);

                }
            }
            else //the point already exists
            {
                row_indices.push_back(i);
                column_indices.push_back(idxY*g.Nx()*g.n() + idxX);
                if( !negative)
                    values.push_back( 1.);
                else
                    values.push_back(-1.);
            }

        }
    }
    else
    {
        unsigned points_per_line = 1;
        if( method == "nearest")
            points_per_line = 1;
        else if( method == "linear")
            points_per_line = 2;
        else if( method == "cubic")
            points_per_line = 4;
        else
            throw std::runtime_error( "Interpolation method "+method+" not recognized!\n");
        RealGrid1d<real_type> gx(g.x0(), g.x1(), g.n(), g.Nx());
        RealGrid1d<real_type> gy(g.y0(), g.y1(), g.n(), g.Ny());
        thrust::host_vector<real_type> absX = dg::create::abscissas( gx);
        thrust::host_vector<real_type> absY = dg::create::abscissas( gy);
        for( unsigned i=0; i<x.size(); i++)
        {
            real_type X = x[i], Y = y[i];
            bool negative = false;
            g.shift( negative, X, Y, bcx, bcy);

            unsigned col_beginX = 0, col_beginY = 0;
            std::vector<real_type> xs  = detail::choose_1d_abscissas( X,
                    points_per_line, gx, absX, col_beginX);
            std::vector<real_type> ys  = detail::choose_1d_abscissas( Y,
                    points_per_line, gy, absY, col_beginY);

            //evaluate 2d Legendre polynomials at (xn, yn)...
            std::vector<real_type> pxy( points_per_line*points_per_line);
            std::vector<real_type> px = detail::lagrange( X, xs),
                                   py = detail::lagrange( Y, ys);
            for(unsigned k=0; k<py.size(); k++)
                for( unsigned l=0; l<px.size(); l++)
                    pxy[k*px.size()+l]= py[k]*px[l];
            for( unsigned k=0; k<points_per_line; k++)
                for( unsigned l=0; l<points_per_line; l++)
                {
                    if( fabs(pxy[k*points_per_line +l]) > 1e-14)
                    {
                        row_indices.push_back( i);
                        column_indices.push_back( (col_beginY+k)*g.n()*g.Nx() +
                            col_beginX+l);
                        if( !negative )
                            values.push_back(  pxy[k*points_per_line+l]);
                        else
                            values.push_back( -pxy[k*points_per_line+l]);
                    }
                }
        }
    }
    cusp::coo_matrix<int, real_type, cusp::host_memory> A( x.size(),
            g.size(), values.size());
    A.row_indices = row_indices;
    A.column_indices = column_indices;
    A.values = values;

    return A;
}



/**
 * @brief Create interpolation matrix
 *
 * The created matrix has \c g.size() columns and \c x.size() rows. Per default
 * it uses polynomial interpolation given by the dG polynomials, i.e. the
 * interpolation has order \c g.n() .
 * When applied to a vector the result contains the interpolated values at the
 * given interpolation points.
 * @snippet topology/interpolation_t.cu doxygen3d
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @param x X-coordinates of interpolation points
 * @param y Y-coordinates of interpolation points (\c y.size() must equal \c x.size())
 * @param z Z-coordinates of interpolation points (\c z.size() must equal \c x.size())
 * @param g The Grid on which to operate
 * @copydoc hide_bcx_doc
 * @param bcy analogous to \c bcx, applies to y direction
 * @param bcz analogous to \c bcx, applies to z direction
 * @copydoc hide_method
 *
 * @return interpolation matrix
 * @attention all points (x, y, z) must lie within or on the boundaries of g
 */
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation(
        const thrust::host_vector<real_type>& x,
        const thrust::host_vector<real_type>& y,
        const thrust::host_vector<real_type>& z,
        const aRealTopology3d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU, dg::bc bcz = dg::PER,
        std::string method = "dg")
{
    assert( x.size() == y.size());
    assert( y.size() == z.size());
    cusp::array1d<real_type, cusp::host_memory> values;
    cusp::array1d<int, cusp::host_memory> row_indices;
    cusp::array1d<int, cusp::host_memory> column_indices;

    if( method == "dg")
    {
        std::vector<real_type> gauss_nodes = g.dlt().abscissas();
        dg::Operator<real_type> forward( g.dlt().forward());
        for( int i=0; i<(int)x.size(); i++)
        {
            real_type X = x[i], Y = y[i], Z = z[i];
            bool negative = false;
            g.shift( negative,X,Y,Z, bcx, bcy, bcz);

            //determine which cell (x,y) lies in
            real_type xnn = (X-g.x0())/g.hx();
            real_type ynn = (Y-g.y0())/g.hy();
            real_type znn = (Z-g.z0())/g.hz();
            unsigned nn = (unsigned)floor(xnn);
            unsigned mm = (unsigned)floor(ynn);
            unsigned ll = (unsigned)floor(znn);
            //determine normalized coordinates
            real_type xn = 2.*xnn - (real_type)(2*nn+1);
            real_type yn = 2.*ynn - (real_type)(2*mm+1);
            //interval correction
            if (nn==g.Nx()) {
                nn-=1;
                xn = 1.;
            }
            if (mm==g.Ny()) {
                mm-=1;
                yn =1.;
            }
            if (ll==g.Nz()) {
                ll-=1;
            }
            //Test if the point is a Gauss point since then no interpolation is needed
            int idxX =-1, idxY = -1;
            for( unsigned k=0; k<g.n(); k++)
            {
                if( fabs( xn - gauss_nodes[k]) < 1e-14)
                    idxX = nn*g.n() + k; //determine which grid column it is
                if( fabs( yn - gauss_nodes[k]) < 1e-14)
                    idxY = mm*g.n() + k;  //determine grid line
            } //in z-direction we don't interpolate
            if( idxX < 0 && idxY < 0 ) //there is no corresponding point
            {
                //evaluate 2d Legendre polynomials at (xn, yn)...
                std::vector<real_type> px = detail::coefficients( xn, g.n()),
                                    py = detail::coefficients( yn, g.n());
                std::vector<real_type> pxF(g.n(),0), pyF(g.n(), 0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                    {
                        pxF[l]+= px[k]*forward(k,l);
                        pyF[l]+= py[k]*forward(k,l);
                    }
                std::vector<real_type> pxyz( g.n()*g.n());
                //these are the matrix coefficients with which to multiply
                for(unsigned k=0; k<pyF.size(); k++)
                    for( unsigned l=0; l<pxF.size(); l++)
                        pxyz[k*g.n()+l]= 1.*pyF[k]*pxF[l];
                for( unsigned k=0; k<g.n(); k++)
                    for( unsigned l=0; l<g.n(); l++)
                    {
                        row_indices.push_back( i);
                        column_indices.push_back(
                                ((ll*g.Ny()+mm)*g.n()+k)*g.n()*g.Nx()+nn*g.n()
                                + l);
                        if( !negative)
                            values.push_back( pxyz[k*g.n()+l]);
                        else
                            values.push_back(-pxyz[k*g.n()+l]);
                    }
            }
            else if ( idxX < 0 && idxY >=0) //there is a corresponding line
            {
                std::vector<real_type> px = detail::coefficients( xn, g.n());
                std::vector<real_type> pxF(g.n(),0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                        pxF[l]+= px[k]*forward(k,l);
                for( unsigned l=0; l<g.n(); l++)
                {
                    row_indices.push_back( i);
                    column_indices.push_back( (ll*g.Ny()*g.n() +
                                idxY)*g.Nx()*g.n() + nn*g.n() + l);
                    if( !negative)
                        values.push_back( pxF[l]);
                    else
                        values.push_back(-pxF[l]);
                }
            }
            else if ( idxX >= 0 && idxY < 0) //there is a corresponding column
            {
                std::vector<real_type> py = detail::coefficients( yn, g.n());
                std::vector<real_type> pyF(g.n(),0);
                for( unsigned l=0; l<g.n(); l++)
                    for( unsigned k=0; k<g.n(); k++)
                        pyF[l]+= py[k]*forward(k,l);
                for( unsigned k=0; k<g.n(); k++)
                {
                    row_indices.push_back(i);
                    column_indices.push_back(((ll*g.Ny()+mm)*g.n()+k)*g.Nx()*g.n() + idxX);
                    if(!negative)
                        values.push_back( pyF[k]);
                    else
                        values.push_back(-pyF[k]);
                }
            }
            else //the point already exists
            {
                row_indices.push_back(i);
                column_indices.push_back((ll*g.Ny()*g.n()+idxY)*g.Nx()*g.n() + idxX);
                if( !negative)
                    values.push_back( 1.);
                else
                    values.push_back(-1.);
            }

        }
    }
    else
    {
        unsigned points_per_line = 1;
        if( method == "nearest")
            points_per_line = 1;
        else if( method == "linear")
            points_per_line = 2;
        else if( method == "cubic")
            points_per_line = 4;
        else
            throw std::runtime_error( "Interpolation method "+method+" not recognized!\n");
        RealGrid1d<real_type> gx(g.x0(), g.x1(), g.n(), g.Nx());
        RealGrid1d<real_type> gy(g.y0(), g.y1(), g.n(), g.Ny());
        RealGrid1d<real_type> gz(g.z0(), g.z1(), 1,     g.Nz());
        thrust::host_vector<real_type> absX = dg::create::abscissas( gx);
        thrust::host_vector<real_type> absY = dg::create::abscissas( gy);
        thrust::host_vector<real_type> absZ = dg::create::abscissas( gz);
        for( unsigned i=0; i<x.size(); i++)
        {
            real_type X = x[i], Y = y[i], Z = z[i];
            bool negative = false;
            g.shift( negative, X, Y, Z, bcx, bcy, bcz);

            unsigned col_beginX = 0, col_beginY = 0, col_beginZ = 0;
            std::vector<real_type> xs  = detail::choose_1d_abscissas( X,
                    points_per_line, gx, absX, col_beginX);
            std::vector<real_type> ys  = detail::choose_1d_abscissas( Y,
                    points_per_line, gy, absY, col_beginY);
            std::vector<real_type> zs  = detail::choose_1d_abscissas( Z,
                    points_per_line, gz, absZ, col_beginZ);

            //evaluate 2d Legendre polynomials at (xn, yn)...
            std::vector<real_type> pxyz( points_per_line*points_per_line
                    *points_per_line);
            std::vector<real_type> px = detail::lagrange( X, xs),
                                   py = detail::lagrange( Y, ys),
                                   pz = detail::lagrange( Z, zs);
            for( unsigned m=0; m<pz.size(); m++)
            for( unsigned k=0; k<py.size(); k++)
            for( unsigned l=0; l<px.size(); l++)
                pxyz[(m*py.size()+k)*px.size()+l]= pz[m]*py[k]*px[l];
            for( unsigned m=0; m<points_per_line; m++)
            for( unsigned k=0; k<points_per_line; k++)
            for( unsigned l=0; l<points_per_line; l++)
            {
                if( fabs(pxyz[(m*points_per_line+k)*points_per_line +l]) > 1e-14)
                {
                    row_indices.push_back( i);
                    column_indices.push_back( ((col_beginZ + m)*g.n()*g.Ny() +
                                col_beginY+k)*g.n()*g.Nx() + col_beginX+l);
                    if( !negative )
                        values.push_back(
                                pxyz[(m*points_per_line+k)*points_per_line+l]);
                    else
                        values.push_back(
                               -pxyz[(m*points_per_line+k)*points_per_line+l]);
                }
            }
        }
    }
    cusp::coo_matrix<int, real_type, cusp::host_memory> A( x.size(), g.size(),
            values.size());
    A.row_indices = row_indices;
    A.column_indices = column_indices;
    A.values = values;

    return A;
}
/**
 * @brief Create interpolation between two grids
 *
 * This matrix interpolates vectors on the old grid \c g_old to the %Gaussian nodes of the new grid \c g_new. The interpolation is of the order \c g_old.n()
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @sa for integer multiples between old and new %grid you may want to consider the dg::create::fast_interpolation %functions
 *
 * @param g_new The new grid
 * @param g_old The old grid
 *
 * @return Interpolation matrix with \c g_old.size() columns and \c g_new.size() rows
 * @note The boundaries of the old grid must lie within the boundaries of the new grid
 * @note When interpolating a 2d grid to a 3d grid the third coordinate is simply ignored, i.e. the 2d vector will be trivially copied Nz times into the 3d vector
 * @note also check the transformation matrix, which is the more general solution
 */
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation( const RealGrid1d<real_type>& g_new, const RealGrid1d<real_type>& g_old)
{
    //assert both grids are on the same box
    assert( g_new.x0() >= g_old.x0());
    assert( g_new.x1() <= g_old.x1());
    thrust::host_vector<real_type> pointsX = dg::evaluate( dg::cooX1d, g_new);
    return interpolation( pointsX, g_old);

}
///@copydoc interpolation(const RealGrid1d<real_type>&,const RealGrid1d<real_type>&)
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation( const aRealTopology2d<real_type>& g_new, const aRealTopology2d<real_type>& g_old)
{
    //assert both grids are on the same box
    assert( g_new.x0() >= g_old.x0());
    assert( g_new.x1() <= g_old.x1());
    assert( g_new.y0() >= g_old.y0());
    assert( g_new.y1() <= g_old.y1());
    thrust::host_vector<real_type> pointsX = dg::evaluate( dg::cooX2d, g_new);

    thrust::host_vector<real_type> pointsY = dg::evaluate( dg::cooY2d, g_new);
    return interpolation( pointsX, pointsY, g_old);

}

///@copydoc interpolation(const RealGrid1d<real_type>&,const RealGrid1d<real_type>&)
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation( const aRealTopology3d<real_type>& g_new, const aRealTopology3d<real_type>& g_old)
{
    //assert both grids are on the same box
    assert( g_new.x0() >= g_old.x0());
    assert( g_new.x1() <= g_old.x1());
    assert( g_new.y0() >= g_old.y0());
    assert( g_new.y1() <= g_old.y1());
    assert( g_new.z0() >= g_old.z0());
    assert( g_new.z1() <= g_old.z1());
    thrust::host_vector<real_type> pointsX = dg::evaluate( dg::cooX3d, g_new);
    thrust::host_vector<real_type> pointsY = dg::evaluate( dg::cooY3d, g_new);
    thrust::host_vector<real_type> pointsZ = dg::evaluate( dg::cooZ3d, g_new);
    return interpolation( pointsX, pointsY, pointsZ, g_old);

}
///@copydoc interpolation(const RealGrid1d<real_type>&,const RealGrid1d<real_type>&)
template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> interpolation( const aRealTopology3d<real_type>& g_new, const aRealTopology2d<real_type>& g_old)
{
    //assert both grids are on the same box
    assert( g_new.x0() >= g_old.x0());
    assert( g_new.x1() <= g_old.x1());
    assert( g_new.y0() >= g_old.y0());
    assert( g_new.y1() <= g_old.y1());
    thrust::host_vector<real_type> pointsX = dg::evaluate( dg::cooX3d, g_new);
    thrust::host_vector<real_type> pointsY = dg::evaluate( dg::cooY3d, g_new);
    return interpolation( pointsX, pointsY, g_old);

}
///@}


}//namespace create

/**
 * @brief Transform a vector from dg::xspace (nodal values) to dg::lspace (modal values)
 *
 * @param in input
 * @param g grid
 *
 * @ingroup misc
 * @return the vector in LSPACE
 */
template<class real_type>
thrust::host_vector<real_type> forward_transform( const thrust::host_vector<real_type>& in, const aRealTopology2d<real_type>& g)
{
    thrust::host_vector<real_type> out(in.size(), 0);
    dg::Operator<real_type> forward( g.dlt().forward());
    for( unsigned i=0; i<g.Ny(); i++)
    for( unsigned k=0; k<g.n(); k++)
    for( unsigned j=0; j<g.Nx(); j++)
    for( unsigned l=0; l<g.n(); l++)
    for( unsigned m=0; m<g.n(); m++)
    for( unsigned o=0; o<g.n(); o++)
        out[((i*g.n() + k)*g.Nx() + j)*g.n() + l] += forward(k,o)*forward( l, m)*in[((i*g.n() + o)*g.Nx() + j)*g.n() + m];
    return out;
}


/**
 * @brief Interpolate a vector on a single point on a 1d Grid
 *
 * @param sp Indicate whether the elements of the vector
 * v are in xspace (nodal values) or lspace (modal values)
 *  (choose dg::xspace if you don't know what is going on here,
 *      It is faster to interpolate in dg::lspace so consider
 *      transforming v using dg::forward_transform( )
 *      if you do it very many times)
 * @param v The vector to interpolate
 * @param x X-coordinate of interpolation point
 * @param g The Grid on which to operate
 * @copydoc hide_bcx_doc
 *
 * @ingroup interpolation
 * @return interpolated point
 */
template<class real_type>
real_type interpolate(
    dg::space sp,
    const thrust::host_vector<real_type>& v,
    real_type x,
    const RealGrid1d<real_type>& g,
    dg::bc bcx = dg::NEU)
{
    assert( v.size() == g.size());
    bool negative = false;
    g.shift( negative, x, bcx);

    //determine which cell (x) lies in

    real_type xnn = (x-g.x0())/g.h();
    unsigned n = (unsigned)floor(xnn);
    //determine normalized coordinates

    real_type xn =  2.*xnn - (real_type)(2*n+1);
    //interval correction
    if (n==g.N()) {
        n-=1;
        xn = 1.;
    }
    //evaluate 1d Legendre polynomials at (xn)...
    std::vector<real_type> px = create::detail::coefficients( xn, g.n());
    if( sp == dg::xspace)
    {
        dg::Operator<real_type> forward( g.dlt().forward());
        std::vector<real_type> pxF(g.n(),0);
        for( unsigned l=0; l<g.n(); l++)
            for( unsigned k=0; k<g.n(); k++)
                pxF[l]+= px[k]*forward(k,l);
        for( unsigned k=0; k<g.n(); k++)
            px[k] = pxF[k];
    }
    //these are the matrix coefficients with which to multiply
    unsigned col_begin = (n)*g.n();
    //multiply x
    real_type value = 0;
    for( unsigned j=0; j<g.n(); j++)
    {
        if(negative)
            value -= v[col_begin + j]*px[j];
        else
            value += v[col_begin + j]*px[j];
    }
    return value;
}

/**
 * @brief Interpolate a vector on a single point on a 2d Grid
 *
 * @param sp Indicate whether the elements of the vector
 * v are in xspace (nodal values) or lspace  (modal values)
 *  (choose dg::xspace if you don't know what is going on here,
 *      It is faster to interpolate in dg::lspace so consider
 *      transforming v using dg::forward_transform( )
 *      if you do it very many times)
 * @param v The vector to interpolate in dg::xspace, or dg::lspace s.a. dg::forward_transform( )
 * @param x X-coordinate of interpolation point
 * @param y Y-coordinate of interpolation point
 * @param g The Grid on which to operate
 * @copydoc hide_bcx_doc
 * @param bcy analogous to \c bcx, applies to y direction
 *
 * @ingroup interpolation
 * @return interpolated point
 */
template<class real_type>
real_type interpolate(
    dg::space sp,
    const thrust::host_vector<real_type>& v,
    real_type x, real_type y,
    const aRealTopology2d<real_type>& g,
    dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU )
{
    assert( v.size() == g.size());
    bool negative = false;
    g.shift( negative, x,y, bcx, bcy);

    //determine which cell (x,y) lies in

    real_type xnn = (x-g.x0())/g.hx();
    real_type ynn = (y-g.y0())/g.hy();
    unsigned n = (unsigned)floor(xnn);
    unsigned m = (unsigned)floor(ynn);
    //determine normalized coordinates

    real_type xn =  2.*xnn - (real_type)(2*n+1);
    real_type yn =  2.*ynn - (real_type)(2*m+1);
    //interval correction
    if (n==g.Nx()) {
        n-=1;
        xn = 1.;
    }
    if (m==g.Ny()) {
        m-=1;
        yn =1.;
    }
    //evaluate 2d Legendre polynomials at (xn, yn)...
    std::vector<real_type> px = create::detail::coefficients( xn, g.n()),
                           py = create::detail::coefficients( yn, g.n());
    if( sp == dg::xspace)
    {
        dg::Operator<real_type> forward( g.dlt().forward());
        std::vector<real_type> pxF(g.n(),0), pyF(g.n(), 0);
        for( unsigned l=0; l<g.n(); l++)
            for( unsigned k=0; k<g.n(); k++)
            {
                pxF[l]+= px[k]*forward(k,l);
                pyF[l]+= py[k]*forward(k,l);
            }
        for( unsigned k=0; k<g.n(); k++)
            px[k] = pxF[k], py[k] = pyF[k];
    }
    //these are the matrix coefficients with which to multiply
    unsigned col_begin = (m)*g.Nx()*g.n()*g.n() + (n)*g.n();
    //multiply x
    real_type value = 0;
    for( unsigned i=0; i<g.n(); i++)
        for( unsigned j=0; j<g.n(); j++)
        {
            if(negative)
                value -= v[col_begin + i*g.Nx()*g.n() + j]*px[j]*py[i];
            else
                value += v[col_begin + i*g.Nx()*g.n() + j]*px[j]*py[i];
        }
    return value;
}

} //namespace dg
