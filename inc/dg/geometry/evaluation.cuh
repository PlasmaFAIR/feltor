#pragma once

#include <cassert> 
#include <thrust/host_vector.h>
#include "grid.h"

/*! @file 
  @brief Function discretization routines
  */
namespace dg
{
///@cond
namespace create
{
/**
* @brief create host_vector containing 1d X-space abscissas 
*
* same as evaluation of f(x) = x on the grid
* @param g The grid 
*
* @return Host Vector
*/
thrust::host_vector<double> abscissas( const Grid1d& g)
{
    thrust::host_vector<double> v(g.size()); 
    for( unsigned i=0; i<g.N(); i++)
        for( unsigned j=0; j<g.n(); j++)
            v[i*g.n()+j] = (g.x0()+g.h()*(double)i) + (g.h()/2.)*(1. + g.dlt().abscissas()[j]);
    return v;
}
}//
///@endcond

///@addtogroup evaluation
///@{



/**
 * @brief Evaluate a function on gaussian abscissas
 *
 * Evaluates f(x) on the intervall (a,b)
 * @tparam UnaryOp Model of Unary Function
 * @param f The function to evaluate
 * @param g The grid on which to evaluate f
 *
 * @return  A DG Host Vector with values
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @copydoc hide_code_evaluate1d
 */
template< class UnaryOp>
thrust::host_vector<double> evaluate( UnaryOp f, const Grid1d& g)
{
    thrust::host_vector<double> abs = create::abscissas( g);
    for( unsigned i=0; i<g.size(); i++)
        abs[i] = f( abs[i]);
    return abs;
};
///@cond
thrust::host_vector<double> evaluate( double (f)(double), const Grid1d& g)
{
    thrust::host_vector<double> v = evaluate<double (double)>( *f, g);
    return v;
};
///@endcond


/**
 * @brief Evaluate a function on gaussian abscissas
 *
 * Evaluates f(x) on the given grid
 * @copydoc hide_binary
 * @param f The function to evaluate: f = f(x,y)
 * @param g The 2d grid on which to evaluate f
 *
 * @return  A dG Host Vector with values
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @copydoc hide_code_evaluate2d
 */
template< class BinaryOp>
thrust::host_vector<double> evaluate( const BinaryOp& f, const aTopology2d& g)
{
    unsigned n= g.n();
    Grid1d gx(g.x0(), g.x1(), g.n(), g.Nx());
    Grid1d gy(g.y0(), g.y1(), g.n(), g.Ny());
    //thrust::host_vector<double> absx = create::abscissas( gx);
    //thrust::host_vector<double> absy = create::abscissas( gy);
    thrust::host_vector<double> absx( g.n()*g.Nx());
    thrust::host_vector<double> absy( g.n()*g.Ny());
    for( unsigned i=0; i<g.Nx(); i++)
        for( unsigned j=0; j<n; j++)
        {
            double xmiddle = std::fma( g.hx(), (double)(i), g.x0());
            absx[i*n+j] = std::fma( (g.hx()/2.), (1. + g.dlt().abscissas()[j]), xmiddle);
        }
    for( unsigned i=0; i<g.Ny(); i++)
        for( unsigned j=0; j<n; j++)
        {
            double ymiddle = std::fma( g.hy(), (double)(i), g.y0());
            absy[i*n+j] = std::fma( (g.hy()/2.), (1. + g.dlt().abscissas()[j]), ymiddle );
        }

    //exblas::udouble res;
    //res.d = gx.h();
    //std::cout << "hX "<<res.i<<"\n";
    //res.d = gy.h();
    //std::cout << "hY "<<res.i<<"\n";
    //res.d = gx.x0();
    //std::cout << "X0 "<<res.i<<"\n";
    //res.d = gy.x0();
    //std::cout << "Y0 "<<res.i<<"\n";
    //for( int k=0; k<10; k++)
    //{
    //    double interE =  absx[k] ; res.d = interE;
    //    std::cout << "k "<<k<<" AbsX "<<res.i<<"\n";
    //    double interF =  absy[k] ; res.d = interF;
    //    std::cout << "k "<<k<<" AbsY "<<res.i<<"\n";
    //}
    //for( unsigned k=0; k<g.n(); k++)
    //{
    //    double interE = g.dlt().abscissas()[k]; res.d = interE;
    //    std::cout << "k "<<k<<" AbsX "<<res.i<<"\n";
    //}

    thrust::host_vector<double> v( g.size());
    for( unsigned i=0; i<gy.N(); i++)
        for( unsigned k=0; k<n; k++)
            for( unsigned j=0; j<gx.N(); j++)
                for( unsigned r=0; r<n; r++)
                    v[ ((i*n+k)*g.Nx() + j)*n + r] = f( absx[j*n+r], absy[i*n+k]);
    return v;
};
///@cond
thrust::host_vector<double> evaluate( double(f)(double, double), const aTopology2d& g)
{
    return evaluate<double(double, double)>( *f, g);
};
///@endcond

/**
 * @brief Evaluate a function on gaussian abscissas
 *
 * Evaluates f(x,y,z) on the given grid
 * @copydoc hide_ternary
 * @param f The function to evaluate: f = f(x,y,z)
 * @param g The 3d grid on which to evaluate f
 *
 * @return  A dG Host Vector with values
 * @sa <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 * @copydoc hide_code_evaluate3d
 */
template< class TernaryOp>
thrust::host_vector<double> evaluate( const TernaryOp& f, const aTopology3d& g)
{
    unsigned n= g.n();
    Grid1d gx(g.x0(), g.x1(), g.n(), g.Nx());
    Grid1d gy(g.y0(), g.y1(), g.n(), g.Ny());
    Grid1d gz(g.z0(), g.z1(), 1, g.Nz());
    thrust::host_vector<double> absx = create::abscissas( gx);
    thrust::host_vector<double> absy = create::abscissas( gy);
    thrust::host_vector<double> absz = create::abscissas( gz);

    thrust::host_vector<double> v( g.size());
    for( unsigned s=0; s<gz.N(); s++)
        for( unsigned i=0; i<gy.N(); i++)
            for( unsigned k=0; k<n; k++)
                for( unsigned j=0; j<gx.N(); j++)
                    for( unsigned l=0; l<n; l++)
                        v[ (((s*gy.N()+i)*n+k)*g.Nx() + j)*n + l] = f( absx[j*n+l], absy[i*n+k], absz[s]);
    return v;
};
///@cond
thrust::host_vector<double> evaluate( double(f)(double, double, double), const aTopology3d& g)
{
    return evaluate<double(double, double, double)>( *f, g);
};
///@endcond

///@}
}//namespace dg

