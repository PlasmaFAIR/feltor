#error Documentation only
/*! @namespace dg
 * @brief This is the namespace for all functions and
 * classes defined and used by the discontinuous Galerkin library.
 */
/*!
 * @defgroup backend Level 1: Vectors, Matrices and basic operations
 * @{
 *     @defgroup blas Basic container independent subroutines
 *
 *         These routines form the heart of our container free numerical algorithms.
 *         They are called by all our numerical algorithms like conjugate gradient or
 *         time integrators.
 *     @{
 *         @defgroup blas1 BLAS level 1 routines: Vector-Vector
 *              \f$ f( x_{0i}, x_{1i}, x_{2i}, ...) \f$ and \f$ x^T y\f$
 *
 *             Successive calls to blas routines are executed sequentially.
 *             A manual synchronization of threads or devices is never needed
 *             in an application using these functions. All functions returning
 *             a value block until the value is ready.
 *         @defgroup blas2 BLAS level 2 routines: Matrix-Vector
 *              \f$ \alpha M \cdot x + \beta y\f$ and \f$ x^T M y \f$
 *
 *             Successive calls to blas routines are executed sequentially.
 *             A manual synchronization of threads or devices is never needed
 *             in an application using these functions. All functions returning
 *             a value block until the value is ready.
 *         @defgroup tensor Tensor-Vector operations
 *              \f$ v^i = T^{ij} w_j\f$
 *
 *              Although a tensor needs a topology to be well-defined mathematically,
 *              we do not need a grid to perform basic operations computationally.
 *              This is why the tensor operations can appear already in Level 1
 *              of this library.
 *     @}
 *     @defgroup typedefs Useful Typedefs
 *     @defgroup sparsematrix Sparse matrix formats
 *     @defgroup view Vector view
 *     @defgroup mpi_structures MPI backend
 *
 *             In this section the blas functions are implemented for the MPI+X
 *             hardware architectures, where X is e.g. CPU, GPU, accelerator
 *             cards...
 *             The general idea to achieve this is to separate global
 *             communication from local computations and thus readily reuse the
 *             existing, optimized library for the local part. Please consult
 *             the chapter \ref mpi_backend in the introduction for more
 *             details.
 *     @defgroup dispatch The tag dispatch system
 *           Implementation details of \ref dispatch
 * @}
 * @defgroup numerical0 Level 2: Basic numerical algorithms
 * @{
 *     @defgroup time ODE solvers
 *      \f$ \dot y = f(y,t) \f$
 *     @{
 *          @defgroup time_utils Utilities for ODE solvers
 *     @}
 *     @defgroup integration Time integration
 *      \f$ \int_{t_0}^T u(t) dt \f$
 *     @defgroup extrapolation Extrapolation
 *     @defgroup invert Linear and nonlinear solvers
 *     Linear \f$ Ax = b\f$ and non-linear \f$ f(x) = b\f$
 * @}
 * @defgroup geo Level 3: Topology and Geometry
 * @{
 *     @defgroup grid Topological grids and operations
 *
 *          Objects that store topological information (which point is neighbour of
 *          which other point) about the grid.
 *     @{
 *         @defgroup basictopology Topology base classes
 *         @defgroup evaluation evaluate
 *          \f$ f_i = f(x_i) \f$
 *
 *             The function discretisation routines compute the DG discretisation
 *             of analytic functions on a given grid. In 1D the discretisation
 *             simply consists of n function values per grid cell ( where n is the number
 *             of Legendre coefficients used; currently 1 <= n <= 20 ) evaluated at
 *             the Gaussian abscissas in the respective cell. In 2D and 3D we simply
 *             use the product space. We choose x to be the contiguous direction.
 *             The first elements of the resulting vector lie in the cell at (x0,y0) and the last
 *             in (x1, y1).
 *         @defgroup highlevel create weights
 *
 *              overloads for the \c dg::create::weights and \c dg::create::inv_weights functions for all
 *              available topologies
 *         @defgroup creation create derivatives
 *           \f$ D_x\f$, \f$ D_y\f$ and \f$ D_z \f$
 *
 *             High level matrix creation functions
 *         @defgroup interpolation Interpolation and projection
 *          \f$ I \f$ and \f$ P = I^\dagger\f$
 *         @defgroup utilities Averaging
 *         @defgroup scatter Scatter and Gather
 *     @}
 *     @defgroup geometry Geometric grids and tensor operations
 *
 *         These routines form the heart of our geometry free numerical algorithms.
 *         They are called by our geometric operators like the Poisson bracket.
 *     @{
 *         @defgroup basicgeometry Geometry base classes
 *         @defgroup pullback pullback and pushforward
 *          \f$ f_i = f( x (\zeta_i,\eta_i), y(\zeta_i,\eta_i)) \f$
 *         @defgroup metric create volume
 *           \f$ \sqrt{g} \f$
 *         @defgroup generators Grid Generator classes
 *     @}
 *     @defgroup gridtypes Useful Typedefs
 * @}
 * @defgroup numerical1 Level 4: Advanced numerical schemes
 *
 *      These routines make use of both the basic operations as well as the
 *      interfaces defined in the Geometry section.
 * @{
 *     @defgroup arakawa Advection terms
 *          \f$ \vec v \cdot \nabla u\f$ and \f$ \{ f,g\} \f$
 *     @defgroup matrixoperators Matrix operators
 *     Elliptic \f$ -\nabla\cdot (\chi \nabla f)\f$ and Helmholtz \f$ (\chi + \alpha \Delta) f\f$
 *     @defgroup multigrid Multigrid matrix inversion
 *     \f$ A x = b\f$
 * @}
 * @defgroup misc Level 0: Miscellaneous additions
 * @{
 *     @defgroup timer Timer class
 *          t.tic() and T.toc()
 *     @defgroup blas1_helpers Functions and functors for subroutine and evaluate
 *     @{
 *          @defgroup basics Simple
 *              For the dg::evaluate and dg::blas1::evaluate functions
 *
 *          @defgroup functions A large collection
 *              For the dg::evaluate and dg::blas1::evaluate functions
 *
 *          @defgroup composition Composition of two or more functors
 *
 *          @defgroup binary_operators blas1::evaluate binary operators
 *              Binary subroutines for the dg::blas1::evaluate function
 *
 *          @defgroup variadic_evaluates blas1::evaluate variadic functors
 *              Functors to use in the dg::blas1::evaluate function
 *
 *          @defgroup variadic_subroutines blas1::subroutine subroutines
 *              Functors to use in the dg::blas1::subroutine functions
 *     @}
 *     @defgroup lowlevel Lowlevel helper functions and classes
 *
 *         Low level helper routines.
 * @}
 *
 */

/** @class hide_binary
  * @tparam BinaryOp A class or function type with a member/signature equivalent to
  *  - real_type operator()(real_type, real_type) const
  */
/** @class hide_ternary
  * @tparam TernaryOp A class or function type with a member/signature equivalent to
  *  - real_type operator()(real_type, real_type, real_type) const
  */

 /** @class hide_ContainerType
  * @tparam ContainerType
  * Any class for which a specialization of \c TensorTraits exists and which
  * fulfills the requirements of the there defined data and execution policies derived from \c AnyVectorTag and \c AnyPolicyTag.
  * Among others
  *  - <tt> dg::HVec (serial), dg::DVec (cuda / omp), dg::MHVec (mpi + serial) or dg::MDVec (mpi + cuda / omp) </tt>
  *  - <tt> std::vector<dg::DVec> (vector of shared device vectors), std::array<double, 4> (array of 4 doubles)</tt>
  *  - <tt> double (scalar)</tt> and other primitive types ...
  *  .
  * If there are several \c ContainerTypes in the argument list, then \c TensorTraits must exist for all of them
  * \see See \ref dispatch for a detailed explanation of our type %dispatch system
  */
 /** @class hide_matrix
  * @tparam MatrixType
  * Any class for which a specialization of \c TensorTraits exists and which fullfills
  * the requirements of the there defined Matrix tags derived from \c AnyMatrixTag.
  * The \c MatrixType can for example be one of:
  *  - \c scalar or \c Container: Scalars and containers act as diagonal matrices.
  *  - \c dg::HMatrix and \c dg::IHMatrix with \c dg::HVec or \c std::vector<dg::HVec>
  *  - \c dg::DMatrix and \c dg::IDMatrix with \c dg::DVec or \c std::vector<dg::DVec>
  *  - \c dg::MHMatrix with \c dg::MHVec or \c std::vector<dg::MHVec>
  *  - \c dg::MDMatrix with \c dg::MDVec or \c std::vector<dg::MDVec>
  *  -  In case of \c SelfMadeMatrixTag only those \c blas2 functions
  *  that have a corresponding member function in the Matrix class (e.g. \c symv( const ContainerType0&, ContainerType1&); ) can be called.
  *  .
  *  If a \c Container has the \c RecursiveVectorTag, then the \c Matrix is applied to each of the elements.
  */
  /** @class hide_geometry
  * @tparam Geometry
  * A type that is or derives from one of the abstract geometry base classes ( \c aGeometry2d, \c aGeometry3d, \c aMPIGeometry2d, ...).
  */

  /** @class hide_container_geometry
  * @tparam Container
  * A data container class for which the \c blas1 functionality is overloaded and to which the return type of \c blas1::subroutine() can be converted using \c dg::construct.
  * We assume that \c Container is copyable/assignable and has a swap member function.
  * In connection with \c Geometry this is one of
  *  - \c dg::HVec, \c dg::DVec when \c Geometry is a shared memory geometry
  *  - \c dg::MHVec or \c dg::MDVec when \c Geometry is one of the MPI geometries
  * @tparam Geometry
  A type that is or derives from one of the abstract geometry base classes ( \c aGeometry2d, \c aGeometry3d, \c aMPIGeometry2d, ...). \c Geometry determines which \c Container type can be used.
  */

  /** @class hide_geometry_matrix_container
  * @tparam Geometry
  * A type that is or derives from one of the abstract geometry base classes ( \c aGeometry2d, \c aGeometry3d, \c aMPIGeometry2d, ...). \c Geometry determines which \c Matrix and \c Container types can be used:
  * @tparam Matrix
  * A class for which the blas2 functions are callable in connection with the \c Container class and to which the return type of \c create::dx() can be converted using \c dg::blas2::transfer.
  * The \c Matrix type can be one of:
  *  - \c dg::HMatrix with \c dg::HVec and one of the shared memory geometries
  *  - \c dg::DMatrix with \c dg::DVec and one of the shared memory geometries
  *  - \c dg::MHMatrix with \c dg::MHVec and one of the MPI geometries
  *  - \c dg::MDMatrix with \c dg::MDVec and one of the MPI geometries
  * @tparam Container
  * A data container class for which the \c blas1 functionality is overloaded and to which the return type of \c blas1::subroutine() can be converted using \c dg::assign.
  * We assume that \c Container is copyable/assignable and has a swap member function.
  * In connection with \c Geometry this is one of
  *  - \c dg::HVec, \c dg::DVec when \c Geometry is a shared memory geometry
  *  - \c dg::MHVec or \c dg::MDVec when \c Geometry is one of the MPI geometries
  */

/** @class hide_symmetric_op
 * @tparam SymmetricOp
 * A class for which the \c blas2::symv(Matrix&, Vector1&, Vector2&) function is callable
 * with the \c Container type as argument. Also, The functions \c %inv_weights() and \c %precond()
 * need to be callable and return inverse weights and the preconditioner for the conjugate
 * gradient method. \c SymmetricOp is assumed to be linear, symmetric and positive definite!
 * @note you can make your own \c SymmetricOp by providing the member function \c void \c symv(const Container&, Container&);
 * and specializing \c TensorTraits with the \c SelfMadeMatrixTag as the \c tensor_category
 */

/*! @mainpage Introduction
 *
 * @section pdf Introduction to discontinuous Galerkin methods
 * Here is a pdf document explaining the fundamentals of discontinuous Galerkin methods
 *  - <a href="./dg_introduction.pdf" target="_blank">Introduction to dg methods</a>
 *

 * @section dispatch The Level 1 dispatch system
 *
 * Let us first define some nomenclature to ease the following discussion
 *    - \e Scalar: A template parameter T is a Scalar if <tt> typename dg::TensorTraits<T>::tensor_category </tt> exists and derives from \c dg::AnyScalarTag
 *    - \e Vector: A template parameter T is a Vector if it is not a Scalar and if <tt> typename dg::TensorTraits<T>::tensor_category </tt> exists and derives from \c dg::AnyVectorTag
 *    - \e Matrix: A template parameter T is a Matrix if it is not a Scalar or Vector and if <tt> typename  dg::TensorTraits<T>::tensor_category </tt> exists and derives from \c dg::AnyMatrixTag
 *    - \e execution \e policy: A template parameter T has an execution policy if
 *      <tt> typename dg::TensorTraits<T>::execution_policy </tt> exists and derives from \c dg::AnyPolicyTag, The execution policy is \e trivial if it is \c dg::AnyPolicyTag
 *    - \e value \e type : A template parameter T has a value type if
 *      <tt> typename dg::TensorTraits<T>::value_type </tt> exists
 *    - \e compatible: Two vectors are compatible if their tensor_categories both derive from the same base class that itself derives from but is not equal to \c dg::AnyVectorTag, Two execution policies are compatible if they are equal or if at least one of them is trivial.
 *    - \e promote: A Scalar can be promoted to a Vector with all elements equal to the value of the Scalar. A Vector can be promoted to a  Matrix with the Vector being the diagonal and all other elements zero.
 *
 * When dispatching level 1 functions we distinguish between three classes of
 * functions: trivially parallel (\c dg::blas1::subroutine and related \c dg::blas1 functions), global communication
 * (\c dg::blas1::dot and \c dg::blas2::dot) and local communication (\c dg::blas2::symv).
 *
 * @subsection dispatch_evaluate The evaluate function
 *
 * The execution of \c dg::blas1::subroutine with a Functor called \c routine
 * (and all related \c dg::blas1 functions) is equivalent to the following:
 *  -# Assert the following prerequisites:
 *
 *      -# All template parameter types must be either Scalars or Vectors and have an execution policy and a value type
 *      -# All Vectors and all execution policies must be mutually compatible
 *      -# All Vectors must contain the same number of elements (equal sizes)
 *      -# The number of template parameters must be equal to the number of parameters in the \c routine
 *      -# The value type of every template parameter must be convertible to the respective parameter type in the \c routine
 *  -# If all types are Scalars, apply the \c routine and return
 *  -# If at least one type is a Vector, then all Scalars
 *     are promoted to this type with the same size, communicator and execution policy
 *  -# Check the base class of the tensor_category:
 *      -# If \c dg::SharedVectorTag, check execution policy and dispatch to respective implementation (The implementation just loops over all elements in the vectors and applies the \c routine)
 *      -# If \c dg::MPIVectorTag, access the underlying data and recursively call \c dg::blas1::subroutine (and start again at 1)
 *      -# If \c dg::RecursiveVectorTag, loop over all elements and recursively call \c dg::blas1::subroutine for all elements (and start again at 1)
 *
 * @subsection dispatch_dot The dot function
 * The execution of \c dg::blas1::dot and \c dg::blas2::dot is equivalent to the following:
 *  -# Assert the following prerequisites:
 *
 *      -# All template parameter types must be either Scalars or Vectors and have an execution policy and a value type
 *      -# All Vectors and all execution policies must be mutually compatible
 *      -# All Vectors must contain the same number of elements (equal sizes)
 *  -# If all types are Scalars, multiply and return
 *  -# If at least one type is a Vector, then all Scalars
 *     are promoted to this type with the same size, communicator and execution policy
 *  -# Check the base class of the tensor_category:
 *      -# If \c dg::SharedVectorTag, check execution policy and dispatch to respective implementation (The implementation multiplies and accumulates all elements in the vectors). Return the result.
 *      -# If \c dg::MPIVectorTag, assert that the vector MPI-communicators are
 *      \c congruent (same process group, different context) or \c ident (same
 *      process group, same context). Then, access the underlying data and
 *      recursively call \c dot (and start again at 1). Accumulate the result
 *      among participating processes and return.
 *      -# If \c dg::RecursiveVectorTag, loop over all elements and recursively call \c dot for all elements (and start again at 1). Accumulate the results
 *      and return.
 *
 * @subsection dispatch_symv The symv function
 * The execution of the \c dg::blas2::symv (and \c dg::blas2::gemv) functions is hard to discribe in general
 * since each matrix class has individual prerequisites and execution paths.
 * Still, we can identify some general rules:
 *   -# The Matrix type can be either a Scalar (promotes to Scalar times the Unit Matrix), a Vector (promotes to a diagonal Matrix) or a Matrix
 *   -# If the Matrix is either a Scalar or a Vector and the remaining types do not have the \c dg::RecursiveVectorTag tensor category, then \c dg::blas2::symv is equivalent to \c dg::blas1::pointwiseDot
 *   -# If the Matrix has the \c dg::SelfMadeMatrixTag tensor category, then all parameters are immediately forwarded to the \c symv member function. No asserts are performed and none of the following applies.
 *   -# The container template parameters must be Vectors or Scalars and must
 *   have compatible execution policies. Vectors must be compatible.
 *   -# If the tensor category of the Vectors is \c dg::RecursiveVectorTag and
 *   the tensor category of the Matrix is not, then the \c dg::blas2::symv is recursively called with the Matrix on all elements of the Vectors.
 *
 * @section mpi_backend The MPI interface
@note The mpi backend is activated by including \c mpi.h before any other feltor header file
@subsection mpi_vector MPI Vectors and the blas functions

In Feltor each mpi process gets an equally sized chunk of a vector.
The corresponding structure in FELTOR is the \c dg::MPI_Vector, which is
nothing but a wrapper around a container type object and a \c MPI_Comm.
With this the \c dg::blas1 functions can readily be implemented by just redirecting to the
implementation for the container type. The only functions that need
additional
communication are the \c dg::blas1::dot and \c dg::blas2::dot functions (\c MPI_Allreduce).

@subsection mpi_matrix MPI Matrices and the symv function

Contrary to a vector
a matrix can be distributed among processes in two ways:
\a row-wise and \a column-wise.
When we implement a matrix-vector multiplication the order
of communication and computation depends on the distribution
of the matrix.

\subsubsection mpi_row Row distributed matrices

In a row-distributed matrix each process holds the
rows of the matrix that correspond to the portion of the
\c MPI_Vector it holds. Every process thus holds the same amount of rows.
When we implement a matrix-vector multiplication
each process first has to gather
all the elements of the input vector it needs to be able to compute the elements of its output. In general this requires MPI communication.
(read the documentation of \c dg::aCommunicator for more info of how global scatter/gather operations work).
After the elements have been gathered into a buffer the local matrix-vector
multiplications can be executed.
Formally, the gather operation can be written as a matrix \f$G\f$
of \f$1'\f$s and \f$0'\f$s and we write.
\f[
M v = R\cdot G v
\f]
where \f$R\f$ is the row-distributed matrix with modified indices
into a buffer vector
and \f$G\f$ is the gather matrix, in which the MPI-communication takes place.
In this way we achieve a simple split between communication \f$ w=Gv\f$
and computation \f$ Rw\f$. Since the computation of \f$ R w\f$ is entirely local we can reuse the existing
implementation for shared memory systems.
Correspondingly, we define the structure \c dg::MPIDistMat as a simple a wrapper around a
\c LocalMatrix type object
and an instance of a \c dg::aCommunicator.

\subsubsection mpi_column Column distributed matrices

In a column-distributed matrix each process holds the
columns of the matrix that correspond to the portion of the
\c MPI_Vector it holds. Each process thus holds the same amount of columns.
In a column distributed matrix the local matrix-vector multiplication can be executed first because each processor already
has all vector elements it needs.
However the resulting elements have to be communicated back to
the process they belong to. Furthermore, a process has to sum
all elements it receives from other processes on the same
index. This is a scatter and reduce operation and
it can be written as a scatter matrix \f$S\f$ (s.a. \c dg::aCommunicator).
\f[
M v= S\cdot C v
\f]
where \f$S\f$ is the scatter matrix and \f$C\f$ is the column distributed
matrix with modified indices.
Again, we can reuse our shared memory algorithms to implement
the local matrix-vector operation \f$ w=Cv\f$ before the communication
step \f$ S w\f$.
This is also implemented in \c dg::MPIDistMat.

\subsubsection mpi_row_col Row and Column distributed
The \c dg::RowColDistMat goes one step further on a row distributed matrix and separates the matrix \f$ R = R_{inner} + R_{outer}\f$ into
a part that can be computed entirely on the local process and a part that needs communication.
This enables the implementation of overlapping communication and computation. (s.a. \c dg::NearestNeighborComm)
\subsubsection mpi_transpose Transposition
It turns out that a row-distributed matrix can be transposed
by transposition of both the local matrix and the gather matrix (s.a. \c dg::transpose):
\f[ M^\mathrm{T} = G^\mathrm{T} R^\mathrm{T}\f]
The result is then a column distributed matrix.
Analogously, the transpose of a column distributed matrix is a row-distributed matrix.
\subsubsection mpi_create Creation
You can create an MPI row-distributed matrix if you know the global column indices by our \c dg::convert function.
*/
