#pragma once

#include "mpi.h"
#include "average.h"
#include "mpi_grid.h"
#include "mpi_weights.h"

/*! @file
  @brief Classes for poloidal and toroidal average computations.
  */
namespace dg{

/**
 * @brief MPI specialized class for average computations
 *
 * @snippet topology/average_mpit.cu doxygen
 * @ingroup utilities
 */
template< class container>
struct Average<MPI_Vector<container> >
{

    /**
     * @brief Prepare internal workspace
     *
     * @param g the grid from which to take the dimensionality and sizes
     * @param direction the direction or plane over which to average when calling \c operator() (at the moment cannot be \c coo3d::xz or \c coo3d::y)
     */
    Average( const aMPITopology2d& g, enum coo2d direction)
    {
        m_nx = g.local().Nx()*g.n(), m_ny = g.local().Ny()*g.n();
        m_w=dg::construct<MPI_Vector<container>>(dg::create::weights(g, direction));
        m_temp = m_w;
        int remain_dims[] = {false,false}; //true true false
        m_transpose = false;
        unsigned size1d = 0;
        if( direction == dg::coo2d::x)
        {
            dg::blas1::scal( m_w, 1./g.lx());
            size1d = m_ny;
            remain_dims[0] = true;
        }
        else
        {
            m_transpose = true;
            remain_dims[1] = true;
            dg::blas1::scal( m_temp, 1./g.ly());
            dg::transpose( m_nx, m_ny, m_temp.data(), m_w.data());
            size1d = m_nx;
        }

        //Now get the reduction communicator
        MPI_Cart_sub( g.communicator(), remain_dims, &m_comm);
        exblas::mpi_reduce_communicator( m_comm, &m_comm_mod, &m_comm_mod_reduce);
        // ... and the one perpendicular to it
        for( unsigned i=0; i<2; i++)
            remain_dims[i] = !remain_dims[i];
        MPI_Comm comm2;
        MPI_Cart_sub( g.communicator(), remain_dims, &comm2);
        // with that construct the reduce mpi vec
        thrust::host_vector<double> t1d( size1d);
        m_temp1d = MPI_Vector<container>( dg::construct<container>( t1d), comm2);
    }

    ///@copydoc Average()
    Average( const aMPITopology3d& g, enum coo3d direction)
    {
        m_w = dg::construct<MPI_Vector<container>>(dg::create::weights(g, direction));
        m_temp = m_w;
        m_transpose = false;
        unsigned nx = g.n()*g.local().Nx(), ny = g.n()*g.local().Ny(), nz = g.local().Nz();
        int remain_dims[] = {false,false,false};
        m_transpose = false;
        if( direction == dg::coo3d::x) {
            dg::blas1::scal( m_w, 1./g.lx());
            m_nx = nx, m_ny = ny*nz;
            remain_dims[0] = true;
        }
        else if( direction == dg::coo3d::z) {
            m_transpose = true;
            remain_dims[2] = true;
            m_nx = nx*ny, m_ny = nz;
            dg::blas1::scal( m_temp, 1./g.lz());
            dg::transpose( m_nx, m_ny, m_temp.data(), m_w.data());
        }
        else if( direction == dg::coo3d::xy) {
            dg::blas1::scal( m_w, 1./g.lx()/g.ly());
            m_nx = nx*ny, m_ny = nz;
            remain_dims[0] = remain_dims[1] = true;
        }
        else if( direction == dg::coo3d::yz) {
            m_transpose = true;
            m_nx = nx, m_ny = ny*nz;
            remain_dims[1] = remain_dims[2] = true;
            dg::blas1::scal( m_temp, 1./g.ly()/g.lz());
            dg::transpose( m_nx, m_ny, m_temp.data(), m_w.data());
        }
        else
            std::cerr << "Warning: this direction is not implemented\n";
        //Now get the reduction communicator
        MPI_Cart_sub( g.communicator(), remain_dims, &m_comm);
        exblas::mpi_reduce_communicator( m_comm, &m_comm_mod, &m_comm_mod_reduce);
        // ... and the one perpendicular to it
        for( unsigned i=0; i<3; i++)
            remain_dims[i] = !remain_dims[i];
        MPI_Comm comm2;
        MPI_Cart_sub( g.communicator(), remain_dims, &comm2);
        // with that construct the reduce mpi vec
        thrust::host_vector<double> t1d;
        if(!m_transpose)
            t1d = thrust::host_vector<double>( m_ny,0.);
        else
            t1d = thrust::host_vector<double>( m_nx,0.);
        m_temp1d = MPI_Vector<container>( dg::construct<container>( t1d), comm2);
    }
    /**
     * @brief Compute the average as configured in the constructor
     *
     * The compuatation is based on the exact, reproducible scalar product provided in the \c ::exblas library. It is divided in two steps
     *  - average the input field over the direction or plane given in the constructor
     *  - extend the lower dimensional result back to the original dimensionality
     *
     * @param src Source Vector (must have the same size and communicator as the grid given in the constructor)
     * @param res result Vector
     (if \c extend==true, \c res must have same size and communicator as \c src vector, else it gets properly resized, may alias \c src)
     * @param extend if \c true the average is extended back to the original dimensionality and the communicator is the 3d communicator, if \c false, this step is skipped.
     * In that case, each process has a result vector with reduced dimensionality and a Cartesian communicator only in the remaining dimensions. Note that in any case \b all processes get the result (since the underlying dot product distributes its result to all processes)
     */
    void operator() (const MPI_Vector<container>& src, MPI_Vector<container>& res, bool extend = true)
    {
        if( !m_transpose)
        {
            //temp1d has size m_ny
            dg::mpi_average( m_nx, m_ny, src.data(), m_w.data(),
                m_temp1d.data(), m_comm, m_comm_mod, m_comm_mod_reduce);
            if( extend )
                dg::extend_column( m_nx, m_ny, m_temp1d.data(), res.data());
            else
                res = m_temp1d;
        }
        else
        {
            //temp1d has size m_nx
            dg::transpose( m_nx, m_ny, src.data(), m_temp.data());
            dg::mpi_average( m_ny, m_nx, m_temp.data(), m_w.data(), m_temp1d.data(), m_comm, m_comm_mod, m_comm_mod_reduce);
            if( extend )
                dg::extend_line( m_nx, m_ny, m_temp1d.data(), res.data());
            else
                res = m_temp1d;
        }
    }
  private:
    unsigned m_nx, m_ny;
    MPI_Vector<container> m_w, m_temp, m_temp1d;
    bool m_transpose;
    MPI_Comm m_comm, m_comm_mod, m_comm_mod_reduce;
};


}//namespace dg
