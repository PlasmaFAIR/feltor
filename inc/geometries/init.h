#pragma once
//#include "solovev.h"
#include "fluxfunctions.h"
#include "solovev_parameters.h"

/*!@file
 *
 * Initialize and Damping objects
 */
namespace dg
{
namespace geo
{
///@addtogroup profiles
///@{
//
/**
 * @brief Returns zero outside psipmax and inside psipmin, otherwise 1
     \f[ \begin{cases}
        1  \text{ if } \psi_{p,min} < \psi_p(R,Z) < \psi_{p,max}\\
        0  \text{ else}
     \end{cases}\f]
 */
struct Iris : public aCloneableCylindricalFunctor<Iris>
{
    Iris( const CylindricalFunctor& psi, double psi_min, double psi_max ):
        psip_(psi), psipmin_(psi_min), psipmax_(psi_max) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmax_) return 0.;
        if( psip_(R,Z) < psipmin_) return 0.;
        return 1.;
    }
    CylindricalFunctor psip_;
    double psipmin_, psipmax_;
};
/**
 * @brief Returns zero outside psipmaxcut otherwise 1
     \f[ \begin{cases}
        0  \text{ if } \psi_p(R,Z) > \psi_{p,maxcut} \\
        1  \text{ else}
     \end{cases}\f]
 */
struct Pupil : public aCloneableCylindricalFunctor<Pupil>
{
    Pupil( const CylindricalFunctor& psi, double psipmaxcut):
        psip_(psi), psipmaxcut_(psipmaxcut) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmaxcut_) return 0.;
        return 1.;
    }
    CylindricalFunctor psip_;
    double psipmaxcut_;
};
/**
 * @brief Returns psi inside psipmax and psipmax outside psipmax
     \f[ \begin{cases}
        \psi_{p,max}  \text{ if } \psi_p(R,Z) > \psi_{p,max} \\
        \psi_p(R,Z) \text{ else}
     \end{cases}\f]
 */
struct PsiPupil : public aCloneableCylindricalFunctor<PsiPupil>
{
    PsiPupil(const CylindricalFunctor& psi, double psipmax):
        psipmax_(psipmax), psip_(psi) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmax_) return psipmax_;
        return  psip_(R,Z);
    }
    double psipmax_;
    CylindricalFunctor psip_;
};
/**
 * @brief Sets values to one outside psipmaxcut, zero else
     \f[ \begin{cases}
        1  \text{ if } \psi_p(R,Z) > \psi_{p,maxlim} \\
        0  \text{ else}
     \end{cases}\f]
 *
 */
struct PsiLimiter : public aCloneableCylindricalFunctor<PsiLimiter>
{
    PsiLimiter( const CylindricalFunctor& psi, double psipmaxlim):
        psipmaxlim_(psipmaxlim), psip_(psi) { }

    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmaxlim_) return 1.;
        return 0.;
    }
    double psipmaxlim_;
    CylindricalFunctor psip_;
};



/**
 * @brief Damps the outer boundary in a zone
 * from psipmaxcut to psipmaxcut+ 4*alpha with a normal distribution
 * Returns 1 inside, zero outside and a gaussian within
     \f[ \begin{cases}
 1 \text{ if } \psi_p(R,Z) < \psi_{p,max,cut}\\
 0 \text{ if } \psi_p(R,Z) > (\psi_{p,max,cut} + 4\alpha) \\
 \exp\left( - \frac{(\psi_p - \psi_{p,max,cut})^2}{2\alpha^2}\right), \text{ else}
 \end{cases}
   \f]
 *
 */
struct GaussianDamping : public aCloneableCylindricalFunctor<GaussianDamping>
{
    GaussianDamping( const CylindricalFunctor& psi, double psipmaxcut, double alpha):
        psip_(psi), psipmaxcut_(psipmaxcut), alpha_(alpha) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmaxcut_ + 4.*alpha_) return 0.;
        if( psip_(R,Z) < psipmaxcut_) return 1.;
        return exp( -( psip_(R,Z)-psipmaxcut_)*( psip_(R,Z)-psipmaxcut_)/2./alpha_/alpha_);
    }
    CylindricalFunctor psip_;
    double psipmaxcut_, alpha_;
};

/**
 * @brief Damps the inner boundary in a zone
 * from psipmax to psipmax+ 4*alpha with a normal distribution
 * Returns 1 inside, zero outside and a gaussian within
     \f[ \begin{cases}
 1 \text{ if } \psi_p(R,Z) < (\psi_{p,max} - 4\alpha)\\
 0 \text{ if } \psi_p(R,Z) > \psi_{p,max} \\
 \exp\left( - \frac{(\psi_p - \psi_{p,max} + 4\alpha)^2}{2\alpha^2}\right), \text{ else}
 \end{cases}
   \f]
 *
 */
struct GaussianProfDamping : public aCloneableCylindricalFunctor<GaussianProfDamping>
{
    GaussianProfDamping( const CylindricalFunctor& psi, double psipmax, double alpha):
        psip_(psi), psipmax_(psipmax), alpha_(alpha) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > psipmax_ ) return 0.;
        if( psip_(R,Z) < (psipmax_-4.*alpha_)) return 1.;
        return exp( -( psip_(R,Z)-(psipmax_-4.*alpha_))*( psip_(R,Z)-(psipmax_-4.*alpha_))/2./alpha_/alpha_);
    }
    CylindricalFunctor psip_;
    double psipmax_, alpha_;
};
/**
 * @brief Damps the inner boundary in a zone
 * from psipmax to psipmax+ 4*alpha with a normal distribution
 * Returns 1 inside, zero outside and a gaussian within
 * Additionally cuts if Z < Z_xpoint
     \f[ \begin{cases}
 1 \text{ if } \psi_p(R,Z) < (\psi_{p,max} - 4\alpha) \\
 0 \text{ if } \psi_p(R,Z) > \psi_{p,max} || Z < -1.1\varepsilon a  \\
 \exp\left( - \frac{(\psi_p - \psi_{p,max})^2}{2\alpha^2}\right), \text{ else}
 \end{cases}
   \f]
 *
 */
struct GaussianProfXDamping : public aCloneableCylindricalFunctor<GaussianProfXDamping>
{
    GaussianProfXDamping( const CylindricalFunctor& psi, dg::geo::solovev::Parameters gp):
        gp_(gp),
        psip_(psi) { }
    private:
    double do_compute(double R, double Z)const
    {
        if( psip_(R,Z) > gp_.psipmax || Z<-1.1*gp_.elongation*gp_.a) return 0.;
        if( psip_(R,Z) < (gp_.psipmax-4.*gp_.alpha)) return 1.;
        return exp( -( psip_(R,Z)-(gp_.psipmax-4.*gp_.alpha))*( psip_(R,Z)-(gp_.psipmax-4.*gp_.alpha))/2./gp_.alpha/gp_.alpha);
    }
    dg::geo::solovev::Parameters gp_;
    CylindricalFunctor psip_;
};

/**
 * @brief source for quantities N ... dtlnN = ...+ source/N
 * Returns a tanh profile shifted to \f$ \psi_{p,min}-3\alpha\f$
 \f[ 0.5\left( 1 + \tanh\left( -\frac{\psi_p(R,Z) - \psi_{p,min} + 3\alpha}{\alpha}\right)\right) \f]
 */
struct TanhSource : public aCloneableCylindricalFunctor<TanhSource>
{
    TanhSource(const CylindricalFunctor& psi, double psipmin, double alpha):
            psipmin_(psipmin), alpha_(alpha), psip_(psi) { }
    private:
    double do_compute(double R, double Z)const
    {
        return 0.5*(1.+tanh(-(psip_(R,Z)-psipmin_ + 3.*alpha_)/alpha_) );
    }
    double psipmin_, alpha_;
    CylindricalFunctor psip_;
};

// struct Gradient : public aCloneableCylindricalFunctor<Gradient>
// {
//     Gradient(  eule::Parameters p, Parameters gp):
//         p_(p),
//         gp_(gp),
//         psip_(gp) {
//     }
//     private:
//     double do_compute(double R, double Z)
//     {
//         if( psip_(R,Z) < (gp_.psipmin)) return p_.nprofileamp+p_.bgprofamp;
//         if( psip_(R,Z) < 0.) return p_.nprofileamp+p_.bgprofamp-(gp_.psipmin-psip_(R,Z))*(p_.nprofileamp/gp_.psipmin);
//         return p_.bgprofamp;
//     }
//     eule::Parameters p_;
//     Parameters gp_;
//     CylindricalFunctor psip_;
// };

/**
 * @brief Returns density profile with variable peak amplitude and background amplitude
     *\f[ N(R,Z)=\begin{cases}
 A_{bg} + A_{peak}\frac{\psi_p(R,Z)} {\psi_p(R_0, 0)} \text{ if }\psi_p < \psi_{p,max} \\
 A_{bg} \text{ else }
 \end{cases}
   \f]
 */
struct Nprofile : public aCloneableCylindricalFunctor<Nprofile>
{
     Nprofile( double bgprofamp, double peakamp, dg::geo::solovev::Parameters gp, const CylindricalFunctor& psi):
         bgamp(bgprofamp), namp( peakamp),
         gp_(gp),
         psip_(psi) { }
    private:
    double do_compute(double R, double Z)const
    {
        if (psip_(R,Z)<gp_.psipmax) return bgamp +(psip_(R,Z)/psip_(gp_.R_0,0.0)*namp);
	if( psip_(R,Z) > gp_.psipmax || Z<-1.1*gp_.elongation*gp_.a) return bgamp;
        return bgamp;
    }
    double bgamp, namp;
    dg::geo::solovev::Parameters gp_;
    CylindricalFunctor psip_;
};

/**
 * @brief returns zonal flow field
     \f[ N(R,Z)=\begin{cases}
    A_{bg} |\cos(2\pi\psi_p(R,Z) k_\psi)| \text{ if }\psi_p < \psi_{p,max} \\
    0 \text{ else }
 \end{cases}
   \f]
 */
struct ZonalFlow : public aCloneableCylindricalFunctor<ZonalFlow>
{
    ZonalFlow(  double amplitude, double k_psi, dg::geo::solovev::Parameters gp, const CylindricalFunctor& psi):
        amp_(amplitude), k_(k_psi),
        gp_(gp),
        psip_(psi) { }
    private:
    double do_compute(double R, double Z)const
    {
      if (psip_(R,Z)<gp_.psipmax)
          return (amp_*fabs(cos(2.*M_PI*psip_(R,Z)*k_)));
      return 0.;

    }
    double amp_, k_;
    dg::geo::solovev::Parameters gp_;
    CylindricalFunctor psip_;
};


///@}
}//namespace functors
}//namespace dg

