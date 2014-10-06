#pragma once
#include "geometry.h"
/*!@file
 *
 * Initialize and Damping objects
 */
namespace solovev
{
///@addtogroup geom
///@{
/**
 * @brief Returns zero outside psipmax and inside psipmin, otherwise 1
 */ 
struct Iris
{
    Iris( GeomParameters gp ): 
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
    double operator( )(double R, double Z)
    {
        if( psip_(R,Z) > gp_.psipmax) return 0.;
        if( psip_(R,Z) < gp_.psipmin) return 0.;
        return 1.;
    }
    double operator( )(double R, double Z, double phi)
    {
        if( psip_(R,Z,phi) > gp_.psipmax) return 0.;
        if( psip_(R,Z,phi) < gp_.psipmin) return 0.;
        return 1.;
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};
/**
 * @brief Returns zero outside psipmax otherwise 1
 */ 
struct Pupil
{
    Pupil( GeomParameters gp): 
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
    double operator( )(double R, double Z)
    {
        if( psip_(R,Z) > gp_.psipmaxcut) return 0.;
        return 1.;
    }
    double operator( )(double R, double Z, double phi)
    {
        if( psip_(R,Z,phi) > gp_.psipmaxcut) return 0.;
        return 1.;
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};
/**
 * @brief Sets values to one outside psipmaxcut, zero else
 *
 * \f$ 1 \f$, if \f$ \psi_p(R,Z) > \psi_{p,maxcut}\f$
 *
 * \f$ 0 \f$, if \f$ \psi_p(R,Z) < \psi_{p,maxcut}\f$
 */ 
struct PsiLimiter
{
    PsiLimiter( GeomParameters gp): 
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }

    double operator( )(double R, double Z)
    {
        if( psip_(R,Z) > gp_.psipmaxcut) return 1.;
        return 0.;
    }
    double operator( )(double R, double Z, double phi)
    {
        if( psip_(R,Z,phi) > gp_.psipmaxcut) return 1.;
        return 0.;
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief Damps the outer boundary in a zone 
 * from psipmax to psipmax+ 4*alpha with a normal distribution
 * Returns 1 inside, zero outside and a gaussian within
 *
 * \f$ 0 \f$, if \f$ \psi_p(R,Z) > \psi_{p,max} + 4\alpha \f$
 *
 * \f$ 1 \f$, if \f$ \psi_p(R,Z) < \psi_{p,max}\f$
 *
 * \f$ \exp\left( - \frac{(\psi_p - \psi_{p,max})^2}{2\alpha^2}\right)\f$, else
 */ 
struct GaussianDamping
{
    GaussianDamping( GeomParameters gp):
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
    double operator( )(double R, double Z)
    {
        if( psip_(R,Z) > gp_.psipmax + 4.*gp_.alpha) return 0.;
        if( psip_(R,Z) < (gp_.psipmax)) return 1.;
        return exp( -( psip_(R,Z)-gp_.psipmax)*( psip_(R,Z)-gp_.psipmax)/2./gp_.alpha/gp_.alpha);
    }
    double operator( )(double R, double Z, double phi)
    {
        if( psip_(R,Z,phi) > gp_.psipmax + 4.*gp_.alpha) return 0.;
        if( psip_(R,Z,phi) < (gp_.psipmax)) return 1.;
        return exp( -( psip_(R,Z,phi)-gp_.psipmax)*( psip_(R,Z,phi)-gp_.psipmax)/2./gp_.alpha/gp_.alpha);

    }
    private:
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief Returns a tanh profile shifted to psipmax - 3*alpha
 */ 
struct TanhDampingProf
{
    TanhDampingProf( GeomParameters gp):
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) { }
    double operator( )(double R, double Z)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z)-gp_.psipmax + 3.*gp_.alpha)/gp_.alpha) );
    }
    double operator( )(double R, double Z, double phi)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z,phi)-gp_.psipmax + 3.*gp_.alpha)/gp_.alpha) );
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};
/**
 * @brief Returns a tanh profile shifted to psipmaxcut + 3*alpha
 */ 
struct TanhDampingOut
{
        TanhDampingOut( GeomParameters gp):
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) { }
    double operator( )(double R, double Z)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z)-gp_.psipmaxcut - 3.*gp_.alpha)/gp_.alpha) );
    }
    double operator( )(double R, double Z, double phi)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z,phi)-gp_.psipmaxcut - 3.*gp_.alpha)/gp_.alpha) );
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief Returns a tanh profile shifted to psipmaxcut - 3*alpha
 */ 
struct TanhDampingIn
{
        TanhDampingIn( GeomParameters gp):
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
    double operator( )(double R, double Z)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z)-gp_.psipmaxcut + 3.*gp_.alpha)/gp_.alpha) );
    }
    double operator( )(double R, double Z, double phi)
    {
        return 0.5*(1.+tanh(-(psip_(R,Z,phi)-gp_.psipmaxcut + 3.*gp_.alpha)/gp_.alpha) );
    }
    private:
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief source for quantities N ... dtlnN = ...+ source/N
 * Returns a tanh profile shifted to psipmin-3*alpha
 */
struct TanhSource
{
        TanhSource( eule::Parameters p, GeomParameters gp):
        p_(p),
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
    double operator( )(double R, double Z)
    {
        return p_.amp_source*0.5*(1.+tanh(-(psip_(R,Z)-gp_.psipmin + 3.*gp_.alpha)/gp_.alpha) );
    }
    double operator( )(double R, double Z, double phi)
    {
        return p_.amp_source*0.5*(1.+tanh(-(psip_(R,Z,phi)-gp_.psipmin + 3.*gp_.alpha)/gp_.alpha) );
    }
    private:
    eule::Parameters p_;
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief Returns density profile with variable peak amplitude and background amplitude 
 *
 * \f$ N(R,Z) =  A_{bg} + A_{peak}\frac{\psi_p} {\psi_p(R_0, 0)} \f$, for \f$\psi_p <0 \f$ 
 *
 * \f$ N(R,Z) =  A_{bg} \f$, else
 */ 
struct Nprofile
{
     Nprofile( eule::Parameters p, GeomParameters gp):
        p_(p),
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
        }
   double operator( )(double R, double Z)
    {
        if (psip_(R,Z)<0.) return p_.bgprofamp +(psip_(R,Z)/psip_(gp_.R_0,0.0)*p_.nprofileamp);
        return p_.bgprofamp;
    }
    double operator( )(double R, double Z, double phi)
    {
        return (*this)(R,Z);
    }
    private:
    eule::Parameters p_;
    GeomParameters gp_;
    Psip psip_;
};

/**
 * @brief returns zonal flow field 
 */ 
struct ZonalFlow
{
    ZonalFlow(  eule::Parameters p,GeomParameters gp):
        p_(p),
        gp_(gp),
        psip_(Psip(gp.R_0,gp.A,gp.c)) {
    }
    double operator() (double R, double Z)
    {
      if (psip_(R,Z)<0.) return (p_.amp*abs(cos(2.*M_PI*psip_(R,Z)*p_.k_psi)));
      return 0.;

    }
    double operator() (double R, double Z,double phi)
    {
        if (psip_(R,Z,phi)<0.) return ( p_.amp*abs(cos(2.*M_PI*psip_(R,Z,phi)*p_.k_psi)));
        return 0.;
    }
    private:
    GeomParameters gp_;
    eule::Parameters p_;
    Psip psip_;
};

/**
 * @brief testfunction to test the parallel derivative \f[ f = \psi_p(R,Z) \sin{(\varphi)}\f]
 */ 
struct TestFunction
{
    TestFunction(Psip psip) : psip_(psip){}
    double operator()( double R, double Z, double phi)
    {
        return psip_(R,Z,phi)*sin(phi);
    }
    private:
    Psip psip_;
};
/**
 * @brief analyitcal solution of the parallel derivative of the testfunction
 *  \f[ \nabla_\parallel f = \psi_p(R,Z) b^\varphi \cos{(\varphi)}\f]
 */ 
struct DeriTestFunction
{
    DeriTestFunction(GeomParameters gp, Psip psip,PsipR psipR, PsipZ psipZ, Ipol ipol, InvB invB) :gp_(gp), psip_(psip), psipR_(psipR), psipZ_(psipZ),ipol_(ipol), invB_(invB) {}
    double operator()( double R, double Z, double phi)
    {
        return  gp_.R_0*psip_(R,Z,phi)*ipol_(R,Z,phi)*cos(phi)*invB_(R,Z,phi)/R/R;
    }
    private:
    GeomParameters gp_;
    Psip psip_;
    PsipR psipR_;
    PsipZ psipZ_;
    Ipol ipol_;
    InvB invB_;
};

struct DeltaFunction
{
    DeltaFunction(Psip psip, double epsilon,double psivalue) :
         psip_(psip),epsilon_(epsilon),psivalue_(psivalue){
    }
    void setepsilon(double temp ){epsilon_ = temp;}
    void setpsi(double temp ){psivalue_ = temp;}

    double operator()( double R, double Z)
    {
        return 1./sqrt(2.*M_PI*epsilon_)*
               exp( -( (psip_(R,Z)-psivalue_)* (psip_(R,Z)-psivalue_))/2./epsilon_);
    }
    double operator()( double R, double Z, double phi)
    {
        return 1./sqrt(2.*M_PI*epsilon_)*
               exp( -((psip_(R,Z,phi)-psivalue_)* (psip_(R,Z,phi)-psivalue_))/2./epsilon_);
    }
    private:
    Psip psip_;
    double epsilon_;
    double psivalue_;
};
// struct FluxAverage
// {
//     FluxAverage(Psip psip, PsipR psipR,PsipZ psipZ, DeltaFunction deltaf) :
//          psip_(psip),psipR_(psipR),psipZ_(psipZ),deltaf_(deltaf) {
//
//     }
//     double operator()( double psivalue)
//     {
//         deltaf.setpsi(psivalue);
//
//     }
//
//     private:
//     Psip psip_;
//     double epsilon_;
//     double psivalue_;
// };

///@}
}//namespace solovev

