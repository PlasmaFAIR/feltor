#pragma once

#include <cusp/coo_matrix.h>
#include <cusp/print.h>
#include "xspacelib.h"
#ifdef MPI_VERSION
#include "mpi_projection.h" // for convert function
#endif // MPI_VERSION

/*! @file
  @brief Stencil generation
  */
namespace dg
{
namespace create
{
///@cond
namespace detail
{
template<class real_type>
    void set_boundary(
        cusp::array1d<real_type, cusp::host_memory>& values,
        cusp::array1d<int, cusp::host_memory>& column_indices,
        dg::bc bcx,
        int num_cols)
{
    for( unsigned k=0; k<values.size(); k++)
    {
        if( column_indices[k] < 0 )
        {
            if( bcx == dg::NEU || bcx == dg::NEU_DIR)
                column_indices[k] = -(column_indices[k]+1);
            else if( bcx == dg::DIR || bcx == dg::DIR_NEU)
            {
                column_indices[k] = -(column_indices[k]+1);
                values[k] *= -1;
            }
            else if( bcx == dg::PER)
                column_indices[k] += num_cols;
        }
        else if( column_indices[k] >= num_cols)
        {
            if( bcx == dg::NEU || bcx == dg::DIR_NEU)
                column_indices[k] = 2*num_cols-1-column_indices[k];
            else if( bcx == dg::DIR || bcx == dg::NEU_DIR)
            {
                column_indices[k] = 2*num_cols-1-column_indices[k];
                values[k] *= -1;
            }
            else if( bcx == dg::PER)
                column_indices[k] -= num_cols;
        }
    }
}

template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> window_stencil(
        unsigned stencil_size,
        const RealGrid1d<real_type>& local,
        const RealGrid1d<real_type>& global,
        dg::bc bcx = dg::NEU)
{
    cusp::array1d<real_type, cusp::host_memory> values;
    cusp::array1d<int, cusp::host_memory> row_indices;
    cusp::array1d<int, cusp::host_memory> column_indices;

    unsigned num_rows = local.size();
    unsigned num_cols = global.size();
    unsigned radius = stencil_size/2;
    int L0 = round((local.x0() - global.x0())/global.h())*global.n();

    for( unsigned k=0; k<num_rows; k++)
    {
        for( unsigned l=0; l<stencil_size; l++)
        {
            row_indices.push_back( k);
            column_indices.push_back( L0 + (int)(k + l) - (int)radius);
            values.push_back( 1.0);
        }
    }
    set_boundary( values, column_indices, bcx, num_cols);

    cusp::coo_matrix<int, real_type, cusp::host_memory> A(
            num_rows, num_cols, values.size());

    A.row_indices = row_indices;
    A.column_indices = column_indices;
    A.values = values;
    return A;
}

template<class real_type>
cusp::coo_matrix<int, real_type, cusp::host_memory> limiter_stencil(
        const RealGrid1d<real_type>& local,
        const RealGrid1d<real_type>& global,
        dg::bc bcx = dg::NEU)
{
    cusp::array1d<real_type, cusp::host_memory> values;
    cusp::array1d<int, cusp::host_memory> row_indices;
    cusp::array1d<int, cusp::host_memory> column_indices;

    unsigned num_rows = local.size();
    unsigned num_cols = global.size();
    int L0 = round((local.x0() - global.x0())/global.h())*global.n();

    for( unsigned k=0; k<local.N(); k++)
    {
        column_indices.push_back( L0 + (int)(k*global.n()) - (int)global.n());
        column_indices.push_back( L0 + (int)(k*global.n()) );
        column_indices.push_back( L0 + (int)(k*global.n()+1) );
        column_indices.push_back( L0 + (int)(k*global.n()) + (int)global.n());
        for( unsigned j=0; j<4; j++)
        {
            values.push_back( global.n()); // encode n into values array
            row_indices.push_back( k*global.n());
        }
    }
    set_boundary( values, column_indices, bcx, num_cols);

    cusp::coo_matrix<int, real_type, cusp::host_memory> A(
            num_rows, num_cols, values.size());
    A.row_indices = row_indices;
    A.column_indices = column_indices;
    A.values = values;
    A.sort_by_row_and_column();
    return A;
}

template<class real_type>
cusp::coo_matrix< int, real_type, cusp::host_memory> identity_matrix( const RealGrid1d<real_type>& local, const RealGrid1d<real_type>& global)
{
    cusp::coo_matrix<int,real_type,cusp::host_memory> A( local.N(),
            global.N(), local.N());
    int L0 = round((local.x0() - global.x0())/global.h())*global.n();
    for( unsigned i=0; i<local.N(); i++)
    {
        A.row_indices[i] = i;
        A.column_indices[i] = L0 + i;
        A.values[i] = 1.;
    }
    return A;
}

} //namespace detail
///@endcond

///@addtogroup stencil
///@{

/*!
 * @brief A 1d centered window stencil
 *
 * Create a CSR Matrix containing a centered fixed sized window on each row.
 * @param window_size The number of points involved in the window. If even, the
 * number of points left is 1 higher than right.
 * @param g the grid
 * @param bcx Determine what to do at the boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -1 instead of 1.
 * @return A sparse matrix with \c window_size entries per row, each with value 1
 * @tparam real_type The value type of the matrix
 * @sa \c dg::blas2::filtered_symv
 */
template<class real_type>
dg::IHMatrix_t<real_type> window_stencil(
        unsigned window_size,
        const RealGrid1d<real_type>& g,
        dg::bc bcx = dg::NEU)
{
    return detail::window_stencil( window_size, g, g, bcx);
}

/*!
 * @brief A 1d stencil for the dg Slope limiter
 *
 * This stencil is specifically made to implement a dg slope limiter
 * @param g the grid
 * @param bound Determine what to do at the boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -n instead of n.
 * @return A sparse matrix with 0 or 4 entries per row (the zero coefficient has 4 entries, the remaining coefficients have 4), each with value g.n()
 * @tparam real_type The value type of the matrix
 * @sa \c dg::blas2::filtered_symv \c dg::CSRSlopeLimiter
 */
template<class real_type>
dg::IHMatrix_t<real_type> limiter_stencil(
        const RealGrid1d<real_type>& g,
        dg::bc bound = dg::NEU)
{
    return detail::limiter_stencil( g, g, bound);
}



/*!
 * @brief A 2d centered window stencil
 *
 * Create a CSR Matrix containing a centered fixed sized window on each row
 * as the tensor product of two 1d stencils.
 * @param window_size The number of points involved in the window in each dimension.
 * First entry is x-dimension, 2nd is y-dimension.
 * If even, the number of points left is 1 higher than right.
 * @param g the grid
 * @param bcx Determine what to do at the x-boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -1 instead of 1.
 * @param bcy Determine what to do at the y-boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -1 instead of 1.
 * @return A sparse matrix with <tt> window_size[0]*window_size[1] </tt> entries per row, each with value 1
 * @tparam real_type The value type of the matrix
 * @sa \c dg::blas2::filtered_symv
 */
template<class real_type>
dg::IHMatrix_t<real_type> window_stencil(
        std::array<int,2> window_size,
        const aRealTopology2d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU)
{
    auto mx = detail::window_stencil(window_size[0], g.gx(), g.gx(), bcx);
    auto my = detail::window_stencil(window_size[1], g.gy(), g.gy(), bcy);
    return dg::tensorproduct( my, mx);
}

///@copydoc limiter_stencil(const RealGrid1d<real_type>&,dg::bc)
///@param direction The limiter acts on only 1 direction at a time
template<class real_type>
dg::IHMatrix_t<real_type> limiter_stencil(
        enum coo3d direction,
        const aRealTopology2d<real_type>& g,
        dg::bc bound = dg::NEU)
{
    if( direction == dg::coo3d::x)
    {
        auto mx = detail::limiter_stencil(g.gx(), g.gx(), bound);
        auto einsy = detail::identity_matrix( g.gy(), g.gy());
        return dg::tensorproduct( einsy, mx);
    }
    auto my = detail::limiter_stencil(g.gy(), g.gy(), bound);
    auto einsx = detail::identity_matrix( g.gx(), g.gx());
    return dg::tensorproduct( my, einsx);
}

///@copydoc limiter_stencil(const RealGrid1d<real_type>&,dg::bc)
///@param direction The limiter acts on only 1 direction at a time
template<class real_type>
dg::IHMatrix_t<real_type> limiter_stencil(
        enum coo3d direction,
        const aRealTopology3d<real_type>& g,
        dg::bc bound = dg::NEU)
{
    if( direction == dg::coo3d::x)
    {
        auto mx = detail::limiter_stencil(g.gx(), g.gx(), bound);
        auto einsy = detail::identity_matrix( g.gy(), g.gy());
        auto einsz = detail::identity_matrix( g.gz(), g.gz());
        auto temp = dg::tensorproduct( einsy, mx);
        return dg::tensorproduct( einsz, temp);
    }
    if( direction == dg::coo3d::y)
    {
        auto einsx = identity_matrix( g.gx(), g.gx());
        auto my = detail::limiter_stencil(g.gy(), g.gy(), bound);
        auto einsz = identity_matrix( g.gz(), g.gz());
        auto temp =  dg::tensorproduct( my, einsx);
        return dg::tensorproduct( einsz, temp);
    }
    auto mz = detail::limiter_stencil(g.gz(), g.gz(), bound);
    auto einsy = identity_matrix( g.gy(), g.gy());
    auto einsx = identity_matrix( g.gx(), g.gx());
    auto temp =  dg::tensorproduct( einsy, einsx);
    return dg::tensorproduct( mz, temp);
}

/*!
 * @brief A 2d centered window stencil
 *
 * Create a CSR Matrix containing a centered fixed sized window on each row
 * as the tensor product of two 1d stencils and the identity in the third dimension
 * @param window_size The number of points involved in the window in each dimension.
 * First entry is x-dimension, 2nd is y-dimension.
 * If even, the number of points left is 1 higher than right.
 * @param g the grid
 * @param bcx Determine what to do at the x-boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -1 instead of 1.
 * @param bcy Determine what to do at the y-boundary. For Neumann conditions the
 * boundary points are simply duplicated, For Dirichlet they are duplicated
 * as well and the values are set to -1 instead of 1.
 * @return A sparse matrix with <tt> window_size[0]*window_size[1] </tt> entries per row, each with value 1
 * @tparam real_type The value type of the matrix
 * @sa \c dg::blas2::filtered_symv
 */
template<class real_type>
dg::IHMatrix_t<real_type> window_stencil(
        std::array<int,2> window_size,
        const aRealTopology3d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU)
{
    auto mx = detail::window_stencil(window_size[0], g.gx(), g.gx(), bcx);
    auto my = detail::window_stencil(window_size[1], g.gy(), g.gy(), bcy);
    unsigned Nz = g.gz().size();
    cusp::coo_matrix<int,real_type,cusp::host_memory> mz( Nz, Nz, Nz);
    for( unsigned i=0; i<Nz; i++)
    {
        mz.row_indices[i] = mz.column_indices[i] = i;
        mz.values[i] = 1.;
    }

    auto two =  dg::tensorproduct( my, mx);
    return dg::tensorproduct( mz, two);
}

#ifdef MPI_VERSION
///@copydoc dg::create::window_stencil(std::array<int,2>,const aRealTopology2d<real_type>&,dg::bc,dg::bc)
template<class real_type>
dg::MIHMatrix_t<real_type> window_stencil(
        std::array<int,2> window_size,
        const aRealMPITopology2d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU)
{
    auto mx = detail::window_stencil(window_size[0], g.local().gx(), g.global().gx(), bcx);
    auto my = detail::window_stencil(window_size[1], g.local().gy(), g.global().gy(), bcy);
    auto local = dg::tensorproduct( my, mx);
    return dg::convert( (dg::IHMatrix)local, g);
}

///@copydoc window_stencil(std::array<int,2>,const aRealTopology3d<real_type>&,dg::bc,dg::bc)
template<class real_type>
dg::MIHMatrix_t<real_type> window_stencil(
        std::array<int,2> window_size,
        const aRealMPITopology3d<real_type>& g,
        dg::bc bcx = dg::NEU, dg::bc bcy = dg::NEU)
{
    auto mx = detail::window_stencil(window_size[0], g.local().gx(), g.global().gx(), bcx);
    auto my = detail::window_stencil(window_size[1], g.local().gy(), g.global().gy(), bcy);
    auto mz = detail::identity_matrix( g.local().gz(), g.global().gz());
    auto two =  dg::tensorproduct( my, mx);
    auto three = dg::tensorproduct( mz, two);
    return dg::convert( (dg::IHMatrix)three, g);
}

///@copydoc limiter_stencil(const RealGrid1d<real_type>&,dg::bc)
///@param direction The limiter acts on only 1 direction at a time
template<class real_type>
dg::IHMatrix_t<real_type> limiter_stencil(
        enum coo3d direction,
        const aRealMPITopology2d<real_type>& g,
        dg::bc bound = dg::NEU)
{
    if( direction == dg::coo3d::x)
    {
        auto mx = detail::limiter_stencil(g.local().gx(), g.global().gx(), bound);
        auto einsy = detail::identity_matrix( g.local().gy(), g.global().gy());
        auto local = dg::tensorproduct( einsy, mx);
        return dg::convert( (dg::IHMatrix)local, g);
    }
    auto my = detail::limiter_stencil(g.local().gy(), g.global().gy(), bound);
    auto einsx = detail::identity_matrix( g.local().gx(), g.global().gx());
    auto local = dg::tensorproduct( my, einsx);
    return dg::convert( (dg::IHMatrix)local, g);
}

///@copydoc limiter_stencil(const RealGrid1d<real_type>&,dg::bc)
///@param direction The limiter acts on only 1 direction at a time
template<class real_type>
dg::IHMatrix_t<real_type> limiter_stencil(
        enum coo3d direction,
        const aRealMPITopology3d<real_type>& g,
        dg::bc bound = dg::NEU)
{
    if( direction == dg::coo3d::x)
    {
        auto mx = detail::limiter_stencil(g.local().gx(), g.global().gx(), bound);
        auto einsy = detail::identity_matrix( g.local().gy(), g.global().gy());
        auto einsz = detail::identity_matrix( g.local().gz(), g.global().gz());
        auto temp = dg::tensorproduct( einsy, mx);
        auto local = dg::tensorproduct( einsz, temp);
        return dg::convert( (dg::IHMatrix)local, g);
    }
    if( direction == dg::coo3d::y)
    {
        auto einsx = identity_matrix( g.local().gx(), g.global().gx());
        auto my = detail::limiter_stencil(g.local().gy(), g.global().gy(), bound);
        auto einsz = identity_matrix( g.local().gz(), g.global().gz());
        auto temp =  dg::tensorproduct( my, einsx);
        auto local = dg::tensorproduct( einsz, temp);
        return dg::convert( (dg::IHMatrix)local, g);
    }
    auto mz = detail::limiter_stencil(g.local().gz(), g.global().gz(), bound);
    auto einsy = identity_matrix( g.local().gy(), g.global().gy());
    auto einsx = identity_matrix( g.local().gx(), g.global().gx());
    auto temp =  dg::tensorproduct( einsy, einsx);
    auto local = dg::tensorproduct( mz, temp);
    return dg::convert( (dg::IHMatrix)local, g);
}

#endif // MPI_VERSION

///@}
} // namespace create
} // namespace dg
