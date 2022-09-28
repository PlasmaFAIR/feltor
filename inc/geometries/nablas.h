#pragma once
#include <functional>
#include "dg/backend/memory.h"
#include "dg/topology/geometry.h"

#include "../src/feltor/feltor.h"
#include "../src/feltor/parameters.h"
 //#include "feltor/init.h"

 namespace dg
 {
 namespace geo
 {
 ///@addtogroup fluxfunctions
 /**
  * @brief Certain functions related with using Nabla (in divergences, perpendicular gradients, vector dot nabla...)
  * @ingroup misc_geo
  */

 template<class Geometry>
 struct Nablas 
 {
    using geometry_type = Geometry;
 /**
  * @brief Main contructor of Nablas: construct from a 3D geometry to initialize the derivatives and volumes in that grid..
  */

 Nablas(const Geometry& geom3d): m_g(geom3d) {
     dg::blas2::transfer( dg::create::dx( m_g, dg::DIR, dg::centered), m_dR);
     dg::blas2::transfer( dg::create::dy( m_g, dg::DIR, dg::centered), m_dZ);
     m_vol=dg::tensor::volume(m_g.metric());
     m_tmp=m_tmp2=m_vol;
     m_hh=m_g.metric();
     }
    /**
      * @brief Divergence of a perpendicular vector field (input contravariant): \f[ \boldsymbol(\nabla) cdot\boldsymbol{v}=\frac{1}{\sqrt{g}}\partial_i(\sqrt{g}v^i) \f] only in the perpendicular plane.
      * @param v_R Contains the R cotravariant component of the vector v over which to apply the divergence.
      * @param v_Z Contains the Z contravariant component of the vector v over which to apply the divergence.
      * @param F Contains the divergence of the vector v.
      */

 	template<class Container1>		
 	void div (const Container1& v_R, const Container1& v_Z, Container1& F){ //INPUT: CONTRAVARIANT
        dg::blas1::pointwiseDot(v_R, m_vol, m_tmp);
        dg::blas1::pointwiseDot(v_Z, m_vol, m_tmp2);
        dg::blas2::symv( m_dR, m_tmp, F);
        dg::blas2::symv( 1.0,m_dZ, m_tmp2, 1., F);
        dg::blas1::pointwiseDivide(F, m_vol,F);
 }
 /**
      * @brief Vector dot nabla f: gradient in a vector direction (covariant) of a scalar (usually the scalar being different components of a vector): \f[\boldsymbol{v} cdot\boldsymbol{\nabla}f=v_ih^{ij}\partial_j f \f]
      *
      * @param v_R Contains the R covariant component of the vector v that will multiply the gradient.
      * @param v_Z Contains the Z covariant component of the vector v that will multiply the gradient.
      * @param f Contains the scalar function f over which the gradient will be applied and multiply with v.
      * @param F Contains the product between the vector v and the gradient of function f.
      */

    template<class Container1>
    void v_dot_nabla_f (const Container1& v_R, const Container1& v_Z, const Container1& f, Container1& F){ //INPUT: COVARIANT
        dg::blas2::symv( m_dR, f, m_tmp);
        dg::blas2::symv( m_dZ, f, m_tmp2);
        dg::tensor::multiply2d(m_hh, m_tmp, m_tmp2, m_tmp, F); //WE MAKE THE GRADIENT CONTRAVARIANT
        dg::blas1::pointwiseDot(1.0, v_R, m_tmp, 1.0,  v_Z, F, 1.0, F);
    }
 /**
   * @brief Perpendicular gradient of function f (output contravariant): \f[( \boldsymbol{\nabla_\perp)f)^i = h^{ij}\partial_j f \f]
   * @param f Contains the scalar function f over which we will apply the gradient.
   * @param F_R Contains the R contravariant component of the gradient of f.
   * @param F_Z Contains the Z contravariant component of the gradient of f.
   */
 
    template<class Container1>
    void grad_perp_f (const Container1& f, Container1& F_R, Container1& F_Z){ //INPUT: COVARIANT
        dg::blas2::symv( m_dR, f, m_tmp);
        dg::blas2::symv( m_dZ, f, m_tmp2);
        dg::tensor::multiply2d(m_hh, m_tmp, m_tmp2, F_R, F_Z); //WE MAKE THE GRADIENT CONTRAVARIANT
    }
 
    private:
    Geometry m_g;
    dg::SparseTensor<dg::HVec > m_hh;
    dg::HMatrix m_dR;
    dg::HMatrix m_dZ;
    dg::HVec m_vol;
    dg::HVec m_tmp, m_tmp2;
 };
 };//namespace geo
 }//namespace dg
