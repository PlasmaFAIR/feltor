#pragma once

#include "implicit.h"
#include "runge_kutta.h"

namespace dg
{

///@addtogroup time_utils
///@{
/*! @brief Compute \f[ \sqrt{\sum_i x_i^2}\f] using \c dg::blas1::dot
 *
 * The intention of this function is to used in the \c Adaptive timestepping class.
 * @param x Vector to take the norm of
 * @return \c sqrt(dg::blas1::dot(x,x))
* @copydoc hide_ContainerType
 */
template <class ContainerType>
get_value_type<ContainerType> l2norm( const ContainerType& x)
{
    return sqrt( dg::blas1::dot( x,x));
}
/**
 * @brief \f[ h'= h \epsilon_n^{-0.58/p}\epsilon_{n-1}^{0.21/p}\epsilon_{n-2}^{-0.1/p}\f]
 *
 * PID stands for "Proportional" (the present error), "Integral" (the past error), "Derivative" (the future error). See a good tutorial here https://www.youtube.com/watch?v=UR0hOmjaHp0
 * The PID controller is a good controller to start with, it does not overshoot
 * too much, is smooth, has no systematic over- or under-estimation and
 * converges very quickly to the desired timestep
 * @tparam value_type
 * @param dt_old the old (present) timestep
 * @param eps_0 the error relative to the tolerance of the present timestep
 * @param eps_1 the error relative to the tolerance of the previous timestep
 * @param eps_2 the error relative to the tolerance of the second previous timestep
 * @param embedded_order order of the embedded timestepper
 * @param order order of the timestepper
 *
 * @return the new timestep
 */
template<class value_type>
value_type pid_control( value_type dt_old, value_type eps_0, value_type eps_1, value_type eps_2, unsigned embedded_order, unsigned order)
{
    value_type m_k1 = -0.58, m_k2 = 0.21, m_k3 = -0.1;
    value_type factor = pow( eps_0, m_k1/(value_type)order)
                     * pow( eps_1, m_k2/(value_type)order)
                     * pow( eps_2, m_k3/(value_type)order);
    return dt_old*factor;
}
///\f[ h'= h \epsilon_n^{-0.8/p}\epsilon_{n-1}^{0.31/p}\f]
template<class value_type>
value_type pi_control( value_type dt_old, value_type eps_0, value_type eps_1, value_type eps_2, unsigned embedded_order, unsigned order)
{
    value_type m_k1 = -0.8, m_k2 = 0.31;
    value_type factor = pow( eps_0, m_k1/(value_type)order)
                     * pow( eps_1, m_k2/(value_type)order);
    return dt_old*factor;
}
///\f[ h'= h \epsilon_n^{-1/p}\f]
template<class value_type>
value_type i_control( value_type dt_old, value_type eps_0, value_type eps_1, value_type eps_2, unsigned embedded_order, unsigned order)
{
    value_type m_k1 = -1.;
    value_type factor = pow( eps_0, m_k1/(value_type)order);
    return dt_old*factor;
}
///@}

///@cond
template<class value_type>
struct PIDController
{
    PIDController( ){}
    value_type operator()( value_type dt_old, value_type eps_n, value_type eps_n1, value_type eps_n2, unsigned embedded_order, unsigned order)const
    {
        value_type factor = pow( eps_n,  m_k1/(value_type)order)
                         * pow( eps_n1, m_k2/(value_type)order)
                         * pow( eps_n2, m_k3/(value_type)order);
        value_type dt_new = dt_old*std::max( m_lower_limit, std::min( m_upper_limit, factor) );
        return dt_new;
    }
    void set_lower_limit( value_type lower_limit) {
        m_lower_limit = lower_limit;
    }
    void set_upper_limit( value_type upper_limit) {
        m_upper_limit = upper_limit;
    }
    private:
    value_type m_k1 = -0.58, m_k2 = 0.21, m_k3 = -0.1;
    value_type m_lower_limit = 0, m_upper_limit = 1e300;
};
namespace detail{
template<class value_type>
struct Tolerance
{
    // sqrt(size) is norm( 1)
    Tolerance( value_type rtol, value_type atol, value_type size) :
        m_rtol(rtol*sqrt(size)), m_atol( atol*sqrt(size)){}
    DG_DEVICE
    void operator()( value_type previous, value_type& delta) const{
        delta = delta/ ( m_rtol*fabs(previous) + m_atol);
    }
    private:
    value_type m_rtol, m_atol;
};
} //namespace detail
///@endcond

/*!@class hide_stepper
 *
 * @tparam Stepper A timestepper class that computes the actual timestep
 * and an error estimate, for example an embedded Runge Kutta method
 * \c dg::ERKStep or the additive method \c dg::ARKStep. But really,
 * you can also choose to use your own timestepper class. The requirement
 * is that there is a \c step member function that is called as
 * \b stepper.step( rhs, t0, u0, t1, u1, dt, delta)
 * or \b stepper.step( ex, im, t0, u0, t1, u1, dt, delta)
 * depending on whether a purely explicit/implicit or a semi-implicit stepper
 * is used.
 * Here, t0, t1 and dt are of type \b Stepper::value_type, u0,u1 and delta
 * are vector types of type \b Stepper::container_type& and rhs, ex and im are
 * functors implementing the equations that are forwarded from the caller.
 * The parameters t1, u1 and delta are output parameters and must be updated by
 * the stepper.
 * The \c Stepper must have the \c order() and \c embedded_order() member functions that
 * return the (global) order of the method and its error estimate.
  The <tt> const ContainerType& copyable()const; </tt> member must return
  a container of the size that is later used in \c step
  (it does not matter what values \c copyable contains, but its size is important;
  the \c step method can only be called with vectors of this size)
 */

//%%%%%%%%%%%%%%%%%%%Adaptive%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
/*!@brief Driver class for adaptive timestep integration
 *
 * In order to build an adaptive Time integrator you basically need three
 * ingredients: a \c Stepper, a \c ControlFunction and an \c ErrorNorm.
 * The \c Stepper does the actual computation and advances the solution one step further
 * with a given timestep \c dt. Furthermore, it has to come up with an estimate
 * of the error of the solution and indicate the order of that error.
 * With the \c ErrorNorm the error estimate can be converted to a scalar that
 * can be compared to given relative and absolute error tolerances \c rtol and \c atol.
 * Based on the comparison the step is either accepted or rejected. In both cases
 * the \c ControlFunction then comes up with an adapted
 * suggestion for the timestep in the next step, however, if the step was
 * rejected, we make the stepsize decrease by at least 10\%.
 * For more information on these concepts we recommend
 * <a href="http://runge.math.smu.edu/arkode_dev/doc/guide/build/html/Mathematics.html#">the mathematical primer</a> of the ARKode library.
 *
 * For an example on how to use this class in a practical example consider the following code snippet:
 * @snippet multistep_t.cu adaptive
 * @copydoc hide_stepper
 * @note On step rejection, choosing timesteps and introducing restrictions on the controller: here is a quote from professor G. Söderlind (the master of control functions) from a private e-mail conversation:
 *
@note "The issue is that most controllers are best left to do their work without interference. Each time one interferes with the control loop, one creates a transient that has to be dealt with by the controller.

@note It is indeed necessary to reject steps every once in a while, but I usually try to avoid it to the greatest possible extent. In my opinion, the fear of having a too large error on some steps is vastly exaggerated. You see, in some steps the error is too large, and in others it is too small, and all controllers I constructed are designed to be “expectation value correct” in the sense that if the errors are random, the too large and too small errors basically cancel in the long run.

@note Even so, there are times when the error is way out of proportion. But I usually accept an error that is up to, say 2*TOL, which typically won’t cause any problems. Of course, if one hits a sharp change in the solution, the error may be much larger, and the step recomputed. But then one must “reset" the controller, i.e., the controller keeps back information, and it is better to restart the controller to avoid back information to affect the next step."
@attention Should you use this class instead of a fixed stepsize Multistep say?
The thing to consider especially when solving
partial differential equations, is that the right hand side might be very costly
to evaluate. An adaptive stepper (especially one-step methods) usually calls this right hand side more often than a multistep (only one call per step). The additional
computation of the error norm in the Adaptive step might also be important since
the norm requires global communication in a parallel program (bad for scaling
to many nodes).
So does the Adaptive timestepper make up for its increased cost throught the
adaption of the stepsize? In some cases the answer might be a sound Yes.
Especially when
there are velocity peaks in the solution the multistep timestep might be restricted
too much. In other cases when the timestep does not need to be adapted much, a multistep method can be faster.
In any case the big advantage of Adaptive is that it usually just works (even though
it is not fool-proof) and you do not have to spend time finding a suitable timestep
like in the multistep method.
@ingroup time
 */
template<class Stepper>
struct Adaptive
{
    using stepper_type = Stepper;
    using container_type = typename Stepper::container_type; //!< the type of the vector class in use by \c Stepper
    using value_type = typename Stepper::value_type; //!< the value type of the time variable defined by \c Stepper (float or double)
    Adaptive(){}
    /*!@brief Allocate workspace and construct stepper
     * @param ps All parameters are forwarded to the constructor of \c Stepper
     * @tparam StepperParams Type of parameters (deduced by the compiler)
     * @note The workspace for Adaptive is constructed from the \c copyable member
     * of Stepper
     * @note If you do not provide any parameters this will be the default constructor, doing nothing
     */
    template<class ...StepperParams>
    Adaptive(StepperParams&& ...ps): m_stepper(std::forward<StepperParams>(ps)...),
        m_next(m_stepper.copyable()), m_delta(m_stepper.copyable())
    {
        dg::blas1::copy( 1., m_next);
        m_size = dg::blas1::dot( m_next, 1.);
    }
    /**
    * @brief Perfect forward parameters to one of the constructors
    *
    * @tparam Params deduced by the compiler
    * @param ps parameters forwarded to constructors
    */
    template<class ...Params>
    void construct(Params&& ...ps)
    {
        //construct and swap
        *this = Adaptive(  std::forward<Params>(ps)...);
    }
    /*!@brief Guess an initial stepsize
     *
     * If you have wondered what stepsize you should choose in the beginning,
     * don't freak out about it. Really, the initial stepsize is not that
     * important, the stepper does not even have to succeed. Usually the
     * control function will very(!) quickly adapt the stepsize in just one or
     * two steps (even if it's several orders of magnitude off in the beginning).
     *
     * Currently, this function won't do much better than if you just choose a
     * smallish number yourself, but it's there for future improvements.
     */
    template<class Explicit, class ErrorNorm = value_type(const container_type&)>
    value_type guess_stepsize( Explicit& ex, value_type t0, const
            container_type& u0, enum direction dir, ErrorNorm& norm, value_type
            rtol, value_type atol);

    ///@brief Allow write access to internal stepper
    ///
    ///Maybe useful to set options in the stepper
    stepper_type& stepper() { return m_stepper;}
    ///@brief Read access to internal stepper
    const stepper_type& stepper() const { return m_stepper;}

    /*!@brief Explicit or Implicit adaptive step
     *
     * @param rhs The right hand side of the equation to integrate
     * @copydoc hide_adaptive_params
     * @copydoc hide_rhs
     * @copydoc hide_control_error
     */
    template< class RHS,
              class ControlFunction = value_type (value_type, value_type,
                      value_type, value_type, unsigned, unsigned),
              class ErrorNorm = value_type( const container_type&)>
    void step( RHS& rhs,
              value_type t0,
              const container_type& u0,
              value_type& t1,
              container_type& u1,
              value_type& dt,
              ControlFunction& control,
              ErrorNorm& norm,
              value_type rtol,
              value_type atol
              )
    {
        m_stepper.step( rhs, t0, u0, m_t_next, m_next, dt, m_delta);
        return update( t0, u0, t1, u1, dt, control, norm , rtol, atol);
    }
    /*!@brief Semi-implicit adaptive step
     *
     * @copydoc hide_adaptive_params
     * @copydoc hide_explicit_implicit
     * @copydoc hide_control_error
     */
    template< class Explicit,
              class Implicit,
              class ControlFunction = value_type (value_type, value_type,
                      value_type, value_type, unsigned, unsigned),
              class ErrorNorm = value_type( const container_type&)>
    void step( Explicit& ex,
              Implicit& im,
              value_type t0,
              const container_type& u0,
              value_type& t1,
              container_type& u1,
              value_type& dt,
              ControlFunction& control,
              ErrorNorm& norm,
              value_type rtol,
              value_type atol)
    {
        m_stepper.step( ex, im, t0, u0, m_t_next, m_next, dt, m_delta);
        return update( t0, u0, t1, u1, dt, control, norm , rtol, atol);
    }
    ///Return true if the last stepsize in step was rejected
    bool failed() const {
        return m_failed;
    }
    private:
    template<   class ControlFunction = value_type (value_type, value_type,
            value_type, value_type, unsigned, unsigned),
                class ErrorNorm = value_type( const container_type&)>
    void update( value_type t0,
                const container_type& u0,
                value_type& t1,
                container_type& u1,
                value_type& dt,
                ControlFunction& control,
                ErrorNorm& norm,
                value_type rtol,
                value_type atol
              )
    {
        dg::blas1::subroutine( detail::Tolerance<value_type>( rtol, atol,
                    m_size), u0, m_delta);
        value_type eps0 = norm(m_delta);
        if( eps0 > m_reject_limit || std::isnan( eps0) )
        {
            value_type dt_old = dt;
            dt = control( dt, eps0, m_eps1, m_eps2, m_stepper.embedded_order(),
                    m_stepper.order());
            if( fabs( dt) > 0.9*fabs(dt_old))
                dt = 0.9*dt_old;
            //0.9*dt_old is a safety limit
            //that prevents an increase of the timestep in case the stepper fails
            m_failed = true;
            dg::blas1::copy( u0, u1);
            t1 = t0;
        }
        else
        {
            dt = control( dt, eps0, m_eps1, m_eps2, m_stepper.embedded_order(),
                    m_stepper.order());
            m_eps2 = m_eps1;
            m_eps1 = eps0;
            dg::blas1::copy( m_next, u1);
            t1 = m_t_next;
            m_failed = false;
        }
    }
    bool m_failed = false;
    Stepper m_stepper;
    container_type m_next, m_delta;
    value_type m_reject_limit = 2;
    value_type m_size, m_eps1=1, m_eps2=1;
    value_type m_t_next = 0;
};
template<class Stepper>
template<class Explicit, class ErrorNorm>
typename Adaptive<Stepper>::value_type Adaptive<Stepper>::guess_stepsize(
        Explicit& ex, value_type t0, const container_type& u0, enum direction
        dir, ErrorNorm& tol, value_type rtol, value_type atol)
{
    value_type desired_accuracy = rtol*tol(u0) + atol;
    ex( t0, u0, m_next);
    value_type dt = pow(desired_accuracy,
            1./(value_type)m_stepper.order())/tol(m_next);
    if( dir != forward)
        dt*=-1.;
    return dt;
}
/*!@class hide_adaptive_params
 * @param t0 initial time
 * @param u0 initial value at \c t0
 * @param t1 (write only) end time ( equals \c t0+dt on output if the step was accepted, otherwise equals \c t0, may alias \c t0)
 * @param u1 (write only) contains the updated result on output if the step was accepted, otherwise a copy of \c u0 (may alias \c u0)
 * @param dt on input: timestep (see dg::Adaptive::guess_stepsize() for an initial stepsize).
 * On output: stepsize proposed by the controller that can be used to continue the integration in the next step.
 * @param control The control function. Usually \c dg::pid_control is a good choice. The task of the control function is to compute a new timestep size based on the old timestep size, the order of the method and the past error(s)
 * @param norm The error norm. Usually \c dg::l2norm is a good choice, but for
 * very small vector sizes the time for the binary reproducible dot product might become
 * a performance bottleneck. Then it's time for your own implementation.
 * @param rtol the desired relative accuracy. Usually 1e-5 is a good choice.
 * @param atol the desired absolute accuracy. Usually 1e-10 is a good choice.
 * @note Try not to mess with dt. The controller is best left alone and it does a very good job choosing timesteps. But how do I output my solution at certain (equidistant) timesteps? First, think about if you really, really need that. Why is it so bad to have
 * output at non-equidistant timesteps? If you are still firm, then consider
 * using an interpolation scheme (cf. \c dg::Extrapolation). Let choosing the timestep
 * yourself be the very last option if the others are not viable
 * @note For partial differential equations the exact value of \c rtol and \c atol might
 * not be important. Due to the CFL condition there might be a sharp barrier in the
 * range of possible stepsizes and the controller usually does a good job finding
 * it and keeping the timestep "just right". However, don't make \c rtol too small, \c 1e-1 say, since then the controller might
 * get too close to the CFL barrier. The timestepper is still able
 * to crash, mind, even though the chances of that happening are
 * somewhat lower than in a fixed stepsize method.
 */

/*!@class hide_control_error
 *
 * @tparam ControlFunction function or Functor called as dt' = control( dt, eps0, eps1, eps2, order, embedded_order), where all parameters are of type value_type except the last two, which are unsigned.
 * @tparam ErrorNorm function or Functor of type value_type( const ContainerType&)
 */

/**
 * @brief The domain that contains all points
 * @ingroup time_utils
 */
struct EntireDomain
{
    ///@brief always true
    template<class T>
    bool contains( T& t) const { return true;}
};

///@addtogroup time
///@{

/*!@class hide_integrateAdaptive
 *
 * @param rhs The right-hand-side
 * @param t0 initial time
 * @param u0 initial value at \c t0
 * @param t1 (read / write) end time; if the solution leaves the domain
 * contains the last time where the solution still lies within the domain
 * on output
 * @param u1 (write only) contains the result corresponding to t1 on output
 * @param dt The initial timestep guess (if 0 the function chooses something
 * for you). The exact value is not really
 * important, the stepper does not even have to succeed. Usually the
 * control function will very(!) quickly adapt the stepsize in just one or
 * two steps (even if it's several orders of magnitude off in the beginning).
 * @param control The control function. Usually \c dg::pid_control is a good
 * choice. The task of the control function is to compute a new timestep size
 * based on the old timestep size, the order of the method and the past
 * error(s)
 * @param norm The error norm. Usually \c dg::l2norm is a good choice, but for
 * very small vector sizes the time for the binary reproducible dot product
 * might become a performance bottleneck. Then it's time for your own
 * implementation.
 * @param rtol the desired relative accuracy. Usually 1e-5 is a good choice.
 * @param atol the desired absolute accuracy. Usually 1e-10 is a good choice.
 * @param domain (optional) a restriction of the solution space. The integrator
 * checks after every step if the solution is still within the given domain
 * \c domain.contains(u1). If not, the integrator will bisect the exact domain
 * boundary (up to the given tolerances) and return (t1, u1) that lies closest
 * (but within) the domain boundary.
 * @return number of steps
 * @attention The integrator may throw if it detects too small timesteps, too many failures, NaN, Inf, or other non-sanitary behaviour
 * @copydoc hide_rhs
 * @copydoc hide_control_error
 * @tparam Domain Must have the \c contains(const ContainerType&) const member
 * function returning true if the given solution is part of the domain,
 * false else (can for example be \c dg::aRealTopology2d)
 * @copydoc hide_ContainerType
 */

/**
 * @brief Integrates a differential equation using a one-step explicit
 * Timestepper, with adaptive stepsize-control and monitoring the sanity of
 * integration
 *
 * @param adaptive An instance of the Adaptive class
 * @copydoc hide_integrateAdaptive
 */
template< class Adaptive,
          class RHS,
          class ContainerType,
          class ErrorNorm = get_value_type<ContainerType>( const ContainerType&),
          class ControlFunction = get_value_type<ContainerType>
    (get_value_type<ContainerType>, get_value_type<ContainerType>,
     get_value_type<ContainerType>, get_value_type<ContainerType>, unsigned,
     unsigned),
          class Domain = EntireDomain>
int integrateAdaptive(
                      Adaptive& adaptive,
                      RHS& rhs,
                      get_value_type<ContainerType> t0,
                      const ContainerType& u0,
                      get_value_type<ContainerType>& t1,
                      ContainerType& u1,
                      get_value_type<ContainerType> dt,
                      ControlFunction control,
                      ErrorNorm norm,
                      get_value_type<ContainerType> rtol,
                      get_value_type<ContainerType> atol=1e-10,
                      const Domain& domain = EntireDomain()
                      )
{
    using  value_type = get_value_type<ContainerType>;
    value_type t_current = t0, dt_current = dt;
    blas1::copy( u0, u1 );
    ContainerType& current(u1);
    if( t1 == t0)
        return 0;
    bool forward = (t1 - t0 > 0);
    if( dt == 0)
        dt_current = adaptive.guess_stepsize( rhs, t0, u0, forward ?
                dg::forward:dg::backward, norm, rtol, atol);

    int counter =0;
    ContainerType last( u0), delta(u0);
    while( (forward && t_current < t1) || (!forward && t_current > t1))
    {
        //remember last step
        t0 = t_current;
        dg::blas1::copy( current, last);
        if( (forward && t_current+dt_current > t1) || (!forward && t_current +
                    dt_current < t1) )
            dt_current = t1-t_current;
        // Compute a step and error
        adaptive.step( rhs, t_current, current, t_current, current, dt_current,
                control, norm, rtol, atol);
        if( !std::isfinite(dt_current) || fabs(dt_current) < 1e-9*fabs(t1-t0))
            throw dg::Error(dg::Message(_ping_)<<"integrateERK failed to converge! dt = "<<std::scientific<<dt_current);
        counter++;
        if( !domain.contains( current) )
        {
            t1 = t_current; // u1 is uninteresting because outside
            value_type t_middle = (t1+t0)/2.;
            //start bisection between t0 and t1
            dg::blas1::copy( 1., delta);
            unsigned size = norm( delta); //norm gives sqrt
            size*=size;
            int j_max = 50;
            for(int j=0; j<j_max; j++)
            {
                adaptive.stepper().step( rhs, t0, last, t_middle, u1,
                        (t1-t0)/2., delta);

                counter++;
                dg::blas1::axpby(  1., last, -1., u1, delta);
                dg::blas1::subroutine( detail::Tolerance<value_type>( rtol,
                            atol, size), last, delta);
                value_type eps0 = norm(delta);
                if( domain.contains( u1) )
                {
                    t0 = t_middle;
                    dg::blas1::copy( u1, last);
                    if( eps0 < 1.0)
                    {
                        t1 = t0;
                        return counter;
                    }
                }
                else
                {
                    t1 = t_middle;
                    if( eps0 < 1.0)
                        return counter;
                }
            }
            return counter;
        }
    }
    return counter;
}


/**
 * @brief Shortcut for \c dg::integrateAdaptive with an embedded ERK class as timestepper
 * @snippet adaptive_t.cu function
 * @snippet adaptive_t.cu doxygen
 * @param name name of an embedded method that \c ConvertsToButcherTableau
 * @copydoc hide_integrateAdaptive
 */
template<class RHS,
         class ContainerType,
         class ErrorNorm = get_value_type<ContainerType>( const
                 ContainerType&),
         class ControlFunction = get_value_type<ContainerType>
    (get_value_type<ContainerType>, get_value_type<ContainerType>,
     get_value_type<ContainerType>, get_value_type<ContainerType>, unsigned,
     unsigned),
         class Domain = EntireDomain>
int integrateERK( std::string name,
                  RHS& rhs,
                  get_value_type<ContainerType> t0,
                  const ContainerType& u0,
                  get_value_type<ContainerType>& t1,
                  ContainerType& u1,
                  get_value_type<ContainerType> dt,
                  ControlFunction control,
                  ErrorNorm norm,
                  get_value_type<ContainerType> rtol,
                  get_value_type<ContainerType> atol=1e-10,
                  const Domain& domain = EntireDomain()
              )
{
    dg::Adaptive<dg::ERKStep<ContainerType>> pd( name,u0);
    return integrateAdaptive( pd, rhs, t0, u0, t1, u1, dt, control, norm, rtol,
            atol, domain);
}
///@}
}//namespace dg
