#pragma once

#include <cassert>

#include "blas.h"
#include "elliptic.h"

/*!@file
 *
 * @brief contains special differential operators
 */
namespace dg{

/**
 * @brief Matrix class that represents the arbitrary polarization operator
 *
 * @ingroup matrixoperators
 *
 * Unnormed discretization of \f[ (-\nabla \cdot \chi \nabla - \Delta \iota \Delta +  \nabla \cdot\nabla \cdot 2\iota \nabla \nabla ) x \f]
 * where \f$ \chi\f$ is a function and \f$\alpha\f$ a scalar.
 */
 template< class Geometry, class Matrix, class Container>
 struct TensorElliptic
 {
     using container_type = Container;
     using geometry_type = Geometry;
     using matrix_type = Matrix;
     using value_type = get_value_type<Container>;
     ///@brief empty object ( no memory allocation)
     TensorElliptic() {}
     /**
     * @brief Construct \c TensorElliptic operator
     *
     * @param g The grid to use
     * @param dir Direction of the Laplace operator (Note: only dg::centered tested)
     * @param jfactor The jfactor used in the Laplace operator (probably 1 is always the best factor but one never knows...)
     * @note The default value of \f$\chi\f$ and \f$\iota\f$ is one
     */
     TensorElliptic( const Geometry& g, norm no = not_normed, direction dir = dg::centered, value_type jfactor=1.)
     {
         construct( g, no, dir, jfactor);
     }
     /**
     * @brief Construct \c TensorElliptic operator
     *
     * @param g The grid to use
     * @param bcx boundary condition in x
     * @param bcy boundary contition in y
     * @param dir Direction of the Laplace operator (Note: only dg::centered tested)
     * @param jfactor The jfactor used in the Laplace operator (probably 1 is always the best factor but one never knows...)
     * @note The default value of \f$\chi\f$ and \f$\iota\f$ is one
     */
     TensorElliptic( const Geometry& g, bc bcx, bc bcy, norm no = not_normed, direction dir = dg::centered, value_type jfactor=1.)
     {
         construct( g, bcx, bcy, no, dir, jfactor);
     }
     ///@copydoc TensorElliptic::ArbPo(const Geometry&,value_type,direction,value_type)
     void construct( const Geometry& g,  norm no = not_normed, direction dir = dg::centered, value_type jfactor = 1.)
     {
         construct( g, g.bcx(), g.bcy(), no,  dir, jfactor);
     }
     ///@copydoc TensorElliptic::TensorElliptic(const Geometry&,bc,bc,value_type,direction,value_type)
     void construct( const Geometry& g, bc bcx, bc bcy, norm no = not_normed, direction dir = dg::centered, value_type jfactor = 1.)
     {
        m_jfactor=jfactor;
        m_no = no,
        m_laplaceM_chi.construct( g, bcx, bcy, dg::normed, dir, jfactor);
        m_laplaceM_iota.construct( g, bcx, bcy, dg::normed, dir, jfactor);
        dg::blas2::transfer( dg::create::dx( g, inverse( bcx), inverse(dir)), m_leftx);
        dg::blas2::transfer( dg::create::dy( g, inverse( bcy), inverse(dir)), m_lefty);
        dg::blas2::transfer( dg::create::dx( g, bcx, dir), m_rightx);
        dg::blas2::transfer( dg::create::dy( g, bcy, dir), m_righty);
        dg::blas2::transfer( dg::create::jumpX( g, bcx),   m_jumpX);
        dg::blas2::transfer( dg::create::jumpY( g, bcy),   m_jumpY);
        
        dg::assign( dg::evaluate( dg::one, g), m_temp);
        m_tempx = m_tempy = m_tempxy = m_tempyx = m_iota  = m_helper = m_temp;

        dg::assign( dg::create::inv_volume(g),    m_inv_weights);
        dg::assign( dg::create::volume(g),        m_weights);
        dg::assign( dg::create::inv_weights(g),   m_precond);
        m_chi=g.metric();
        m_metric=g.metric();
        m_vol=dg::tensor::volume(m_chi); //sqrt(g)
        dg::tensor::scal( m_chi, m_vol); //m_chi = sqrt(g) g^{ij}
        dg::assign( dg::create::weights(g), m_weights_wo_vol);
        dg::assign( dg::evaluate(dg::one, g), m_sigma);
     }

     ///@copydoc Elliptic::weights()const
     const Container& weights()const {return m_laplaceM_chi.weights();}
     ///@copydoc Elliptic::inv_weights()const
     const Container& inv_weights()const {return m_laplaceM_chi.inv_weights();} 
     /**
     * @brief Preconditioner to use in conjugate gradient solvers
     *
     * multiply result by these coefficients to get the normed result
     * @return the inverse weights without volume
     */
     const Container& precond()const {return m_laplaceM_chi.precond();}        
     /**
     * @brief Set Chi in the above formula
     *
     * @param chi new container
     */
     template<class ContainerType0>
     void set_chi( const ContainerType0& chi) {m_laplaceM_chi.set_chi(chi); }
     /**
     * @brief Set Iota in the above formula
     *
     * @param iota new container
     */
     template<class ContainerType0>
     void set_iota( const ContainerType0& iota) {m_iota=iota; }   
     /**
     * @brief compute the variational of the operator (psi_2 in gf theory): \f[ - \frac{\chi}{2} \left\{|\nabla \phi|^2 + \alpha \chi ( | \nabla^2 \phi |^2 - (\Delta \phi)^2 / 2) \right\}\f]
     *
     * @param phi (e.g. Gamma phi)
     * @param alpha (e.g. tau/2)
     * @param chi (e.g. 1/B^2)
     * @param varphi equals psi_2 in gf theory if phi = gamma_phi
     */
     void variation(const Container& phi, const value_type& alpha, const Container& chi, Container& varphi)  
     {
//         tensor part
        dg::blas2::symv( m_rightx, phi, m_tempx); //R_x*f        
        dg::blas2::symv(-1.0, m_leftx, m_tempx, 0.0, m_helper); //L_x R_x *f   
        dg::blas2::symv(-1.0, m_righty, m_tempx, 0.0, m_tempyx); //R_y R_x*f 
        dg::blas2::symv( m_righty, phi, m_tempy); //R_y*f                
        dg::blas2::symv(-1.0, m_lefty, m_tempy, 0.0, m_temp); //L_y R_y *f   
        dg::blas2::symv(-1.0, m_rightx, m_tempy, 0.0, m_tempxy); //R_x R_y *f
        
        dg::blas2::symv( m_jfactor, m_jumpX, phi, 1., m_helper);
        dg::blas2::symv( m_jfactor, m_jumpY, phi, 1., m_temp);
        
        dg::blas1::pointwiseDot(alpha, m_temp,     m_temp,  alpha, m_helper,  m_helper,  0., varphi);
        dg::blas1::pointwiseDot(alpha, m_tempxy, m_tempxy,  alpha, m_tempyx,  m_tempyx,  1., varphi);
        dg::blas1::pointwiseDot(varphi, chi, varphi);
         
        //laplacian part
        dg::blas2::symv(m_laplaceM_iota, phi, m_temp); 
        dg::blas1::pointwiseDot(alpha/2., chi, m_temp, m_temp, -1., varphi); //varphi-= alpha *chi/2 (lap phi)^2
        
        //elliptic part
        tensor::multiply2d( m_metric, m_tempx, m_tempy, m_temp, m_helper);
        dg::blas1::pointwiseDot( 1., m_temp, m_tempx, 1., m_helper, m_tempy, 0., m_temp); //m_temp = |nabla phi|^2
        dg::blas1::axpby(-0.5, m_temp, -0.5, varphi);
        dg::blas1::pointwiseDot(chi, varphi, varphi);
         

     }

    template<class ContainerType0, class ContainerType1>
    void operator()( const ContainerType0& x, ContainerType1& y){
        symv( 1, x, 0, y);
    }

    template<class ContainerType0, class ContainerType1>
    void symv( const ContainerType0& x, ContainerType1& y){
        symv( 1, x, 0, y);
    }
     /**
     * @brief apply (not_normed = symmetric) operator
     *
     * Computes
     * \f[ y = W\left[-\nabla \cdot \chi \nabla_\perp - \Delta_\perp \iota \Delta_\perp +  2\nabla \cdot\nabla \cdot \iota \nabla_\perp \nabla_\perp \right] x \f] to make the matrix symmetric
     * 
     * @param x lhs (is constant up to changes in ghost cells)
     * @param y rhs contains solution
     * 
     * @Note Note that for cartesian and cylindrical coordinate systems (with the straight field line approximation) the following relation holds  \f[\nabla \cdot\nabla \cdot (\chi \nabla_\perp^2 f) =  \frac{1}{\sqrt{g}}  \partial_j \left\{\partial_i \left[\sqrt{g} \chi P^{ni} \left( \partial_n ( P^{jm} \partial_m f)\right) \right]\right\} \f] where P is the projection tensor
     */
    template<class ContainerType0, class ContainerType1>
    void symv( value_type alpha, const ContainerType0& x, value_type beta, ContainerType1& y)                                                               
     {               
         //tensor term first
        dg::blas2::symv( m_rightx, x, m_helper); //R_x*f        
        dg::blas2::symv(-1.0, m_leftx, m_helper, 0.0, m_tempx); //L_x R_x *f   
        dg::blas2::symv(-1.0, m_righty, m_helper, 0.0, m_tempyx); //R_y R_x*f 
        dg::blas2::symv( m_righty, x, m_helper); //R_y*f                
        dg::blas2::symv(-1.0, m_lefty, m_helper, 0.0, m_tempy); //L_y R_y *f   
        dg::blas2::symv(-1.0, m_rightx, m_helper, 0.0, m_tempxy); //R_x R_y *f 
        
        dg::blas2::symv( m_jfactor, m_jumpX, x, 1., m_tempx);
        dg::blas2::symv( m_jfactor, m_jumpY, x, 1., m_tempy);
        
        //multiply sqrt(g) iota
        dg::blas1::pointwiseDot( 1., m_tempx,  m_iota,  m_vol, 0., m_tempx); 
        dg::blas1::pointwiseDot( 1., m_tempyx, m_iota, m_vol, 0., m_tempyx); 
        dg::blas1::pointwiseDot( 1., m_tempy,  m_iota,  m_vol, 0., m_tempy); 
        dg::blas1::pointwiseDot( 1., m_tempxy, m_iota, m_vol, 0., m_tempxy); 
        
        dg::blas2::symv( m_rightx, m_tempx, m_helper);     
        dg::blas2::symv(-1.0, m_leftx, m_helper, 0.0, m_temp);  //L_x R_x 
        dg::blas2::symv( m_leftx, m_tempyx, m_helper);       
        dg::blas2::symv(-1.0, m_lefty, m_helper, 1.0, m_temp); //L_y L_x 
        dg::blas2::symv( m_righty, m_tempy, m_helper);       
        dg::blas2::symv(-1.0, m_lefty, m_helper, 1.0, m_temp); //L_y R_y
        dg::blas2::symv( m_lefty, m_tempxy, m_helper);       
        dg::blas2::symv(-1.0, m_leftx, m_helper, 1.0, m_temp);   //L_x L_y
        
        dg::blas2::symv( m_jfactor, m_jumpX, m_tempx, 1., m_temp);
        dg::blas2::symv( m_jfactor, m_jumpY, m_tempy, 1., m_temp); 
        
        dg::blas1::pointwiseDivide(m_temp, m_vol, m_temp); //multiply sqrt(g)^(-1)

        //-lap (iota lap f) term
        dg::blas2::symv( m_laplaceM_iota, x, m_tempx);                                    
        dg::blas1::pointwiseDot( m_iota, m_tempx, m_tempx);  
        dg::blas2::symv(-1.0, m_laplaceM_iota, m_tempx, 2.0, m_temp);    
        
        //elliptic term - div (chi nabla f)
        dg::blas2::symv(1.0, m_laplaceM_chi, x, 1., m_temp);       
        
        if( m_no == normed)
            dg::blas1::axpby(alpha, m_temp, beta, y); 
        if( m_no == not_normed)
            dg::blas2::symv(alpha, m_weights, m_temp, beta, y);    

     }         
     
     private:
     bc inverse( bc bound)
     {
        if( bound == DIR) return NEU;
        if( bound == NEU) return DIR;
        if( bound == DIR_NEU) return NEU_DIR;
        if( bound == NEU_DIR) return DIR_NEU;
        return PER;
     }
     direction inverse( direction dir)
     {
        if( dir == forward) return backward;
        if( dir == backward) return forward;
        return centered;
     }
     Elliptic<Geometry, Matrix, Container> m_laplaceM_chi, m_laplaceM_iota;                                                             
     Matrix m_leftx, m_lefty, m_rightx, m_righty, m_jumpX, m_jumpY;
     Container m_temp, m_tempx, m_tempy, m_tempxy, m_tempyx, m_iota, m_helper;                                                                                  
     Container m_weights, m_inv_weights, m_precond, m_weights_wo_vol;
     SparseTensor<Container> m_chi;
     SparseTensor<Container> m_metric;
     Container m_sigma, m_vol;
     norm m_no;
     value_type m_jfactor;
};
///@cond
template< class G, class M, class V>
struct TensorTraits< TensorElliptic<G, M, V> >
{
    using value_type  = get_value_type<V>;
    using tensor_category = SelfMadeMatrixTag;
};
///@endcond


} //namespace dg

