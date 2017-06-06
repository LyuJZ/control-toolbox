/***********************************************************************************
Copyright (c) 2017, Michael Neunert, Markus Giftthaler, Markus Stäuble, Diego Pardo,
Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of ETH ZURICH nor the names of its contributors may be used
      to endorse or promote products derived from this software without specific
      prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
SHALL ETH ZURICH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************/

/*
 * RbdLinearizer.h
 *
 *  Created on: 11.06.2016
 *      Author: mgiftthaler
 *      Modified: Michael Neunert
 *
 *  Description:
 *  System Linearizer dedicated to Articulated Rigid Body Model.
 *  Note: this Linearizer is different to the standard system linearizer
 *  in ct_core. It takes care of additional terms arising from floating-base
 *  systems. If the system is fixed-base, the standard ct_core linearizer
 *  is called.
 *
 */

#ifndef INCLUDE_CT_RBD_SYSTEMS_LINEAR_RBDLINEARIZER_H_
#define INCLUDE_CT_RBD_SYSTEMS_LINEAR_RBDLINEARIZER_H_

#ifndef RBDLINEARIZER_H_
#define RBDLINEARIZER_H_

#include <memory>

#include <ct/core/core.h>
#include <kindr/Core>

template<int N>
struct print_size_as_warning
{
   char operator()() { return N + 256; } //deliberately causing overflow
};

#define DEBUG

namespace ct {
namespace rbd {

template <class SYSTEM>
class RbdLinearizer : public ct::core::SystemLinearizer<SYSTEM::STATE_DIM, SYSTEM::CONTROL_DIM>
{
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	static const bool FLOATING_BASE = SYSTEM::Dynamics::FB;

	typedef std::shared_ptr<RbdLinearizer<SYSTEM> > Ptr;

	static const size_t STATE_DIM = SYSTEM::STATE_DIM;
	static const size_t CONTROL_DIM = SYSTEM::CONTROL_DIM;
	static const size_t NJOINTS = SYSTEM::Dynamics::NJOINTS;

	static_assert(SYSTEM::STATE_DIM == 2*(SYSTEM::Dynamics::NJOINTS + FLOATING_BASE*6),
	         "STATE DIMENSION MISMATCH. RBD LINEARIZER ONLY WORKS FOR PURE RIGID BODY DYNAMICS.");
	static_assert(SYSTEM::CONTROL_DIM == SYSTEM::Dynamics::NJOINTS,
		     "CONTROL DIMENSION MISMATCH. RBD LINEARIZER ONLY WORKS FOR FULL JOINT CONTROLLED SYSTEMS.");


	typedef ct::core::SystemLinearizer<STATE_DIM, CONTROL_DIM> Base;

	typedef ct::core::StateVector<STATE_DIM> state_vector_t;
	typedef ct::core::ControlVector<CONTROL_DIM>  control_vector_t;

	typedef Eigen::Matrix<double, STATE_DIM, STATE_DIM> state_matrix_t;
	typedef Eigen::Matrix<double, STATE_DIM, CONTROL_DIM> state_control_matrix_t;


	RbdLinearizer(std::shared_ptr<SYSTEM> RBDSystem,
			bool doubleSidedDerivative = false):
		Base(RBDSystem, doubleSidedDerivative),
		RBDSystem_(RBDSystem)
	{
		// check if a non-floating base system is a second order system
		if (!FLOATING_BASE &&
			(this->getType() != ct::core::SYSTEM_TYPE::SECOND_ORDER))
		{
			std::cout << "RbdLinearizer.h: Warning, fixed base system not declared as second order system. "
					<< "Normally, fixed base systems are second order systems and RbdLinearizer will treat them like this. "
					<< "To declare system as second order, see ct::core::System" << std::endl;
		}

		// fill default
		this->dFdx_.template topLeftCorner<STATE_DIM/2, STATE_DIM/2>().setZero();
		this->dFdx_.template topRightCorner<STATE_DIM/2, STATE_DIM/2>().setIdentity();

		// fill default
		this->dFdu_.template topRows<STATE_DIM/2>().setZero();
	}

	RbdLinearizer(const RbdLinearizer& arg):
		Base(arg),
		RBDSystem_(std::shared_ptr<SYSTEM> (
				arg.RBDSystem_->clone()))
	{}

	virtual ~RbdLinearizer() override {}

	RbdLinearizer<SYSTEM>* clone() const override
	{
		return new RbdLinearizer<SYSTEM>(*this);
	}


	const state_matrix_t& getDerivativeState(
			const state_vector_t& x, const control_vector_t& u, const double t = 0.0
			) override
	{
		if(!FLOATING_BASE)
		{
			// call standard ct_core linearizer
			return Base::getDerivativeState(x,u,t);
		}
		else
		{

			Base::getDerivativeState(x,u,t);

			// since we express base pose in world but base twist in body coordinates, we have to modify the top part
			kindr::EulerAnglesXyzD eulerXyz(x.template topRows<3>());
			kindr::RotationMatrixD R_WB_kindr(eulerXyz);

			Eigen::Matrix<double, 3, 6> jacAngVel = jacobianOfAngularVelocityMapping(x.template topRows<3>(), x.template segment<3>(STATE_DIM/2)).transpose();

			//this->dFdx_.template block<3,3>(0,0) = -R_WB_kindr.toImplementation() * JacobianOfRotationMultiplyVector( x.template topRows<3>(), R_WB_kindr.toImplementation()*(x.template segment<3>(STATE_DIM/2) ));
			this->dFdx_.template block<3,3>(0,0) = jacAngVel.template block<3,3>(0,0);

			this->dFdx_.template block<3,3>(3,0) = -R_WB_kindr.toImplementation() * JacobianOfRotationMultiplyVector( x.template topRows<3>(), R_WB_kindr.toImplementation()*(x.template segment<3>(STATE_DIM/2+3) ));


			// Derivative Top Row
			// This is the derivative of the orientation with respect to local angular velocity. This is NOT the rotation matrix
			this->dFdx_.template block<3, 3>(0, STATE_DIM/2) = jacAngVel.template block<3,3>(0,3);
			// we prefer to use a combined calculation. The following call would be equivalent but recomputes sines/cosines
			//this->dFdx_.template block<3, 3>(0, STATE_DIM/2) = eulerXyz.getMappingFromLocalAngularVelocityToDiff();

			// This is the derivative of the position with respect to linear velocity. This is simply the rotation matrix
			this->dFdx_.template block<3, 3>(3, STATE_DIM/2+3) = R_WB_kindr.toImplementation();

			return this->dFdx_;
		}
	}

	const state_control_matrix_t& getDerivativeControl(const state_vector_t& x, const control_vector_t& u, const double t = 0.0) override
	{
		const jsim_t& M  = RBDSystem_->dynamics().kinematics().robcogen().jSim().update(x.template segment<NJOINTS>(FLOATING_BASE*6));

		llt_.compute(M);

		const typename jsim_t::MatrixType& M_inv = llt_.solve(jsim_t::Identity());

	#ifdef DEBUG
		typename jsim_t::MatrixType Meigen = M;
		if (!Meigen.inverse().isApprox(M_inv))
		{
			Eigen::SelfAdjointEigenSolver<typename jsim_t::MatrixType> eigensolver(M);
			std::cout << "The eigenvalues of M are:\n" << eigensolver.eigenvalues().transpose() << std::endl;
			std::cout << "M inverse incorrect" <<std::endl;
			std::cout << "M.inverse: " <<std::endl<<Meigen.inverse()<<std::endl;
			std::cout << "M_inv: " <<std::endl<<M_inv<<std::endl;

			// Marco's LLT version
			jsim_t M_temp = M;
			M_temp.computeL();
			M_temp.computeInverse();
	        auto M_inv_robcogen = M_temp.getInverse();
			std::cout << "M_inv_robcogen: " << std::endl << M_inv_robcogen << std::endl;

		}
	#endif //DEBUG

		auto& S = RBDSystem_->dynamics().S();

		this->dFdu_.template block<STATE_DIM/2, CONTROL_DIM>(STATE_DIM/2, 0) = M_inv * S.transpose();

		return this->dFdu_;
	}



protected:
	typedef typename SYSTEM::Dynamics::ROBCOGEN::JSIM jsim_t;

	std::shared_ptr<SYSTEM> RBDSystem_;

	Eigen::LLT<typename jsim_t::MatrixType> llt_;

private:

	// auto generated code
	Eigen::Matrix3d JacobianOfRotationMultiplyVector(const Eigen::Vector3d& theta, const Eigen::Vector3d& vector)  {

	    const double& theta_1 = theta(0);
	    const double& theta_2 = theta(1);
	    const double& theta_3 = theta(2);

	    const double& vector_1 = vector(0);
	    const double& vector_2 = vector(1);
	    const double& vector_3 = vector(2);


	    Eigen::Matrix3d A0;
	    A0.setZero();


	    double t2 = cos(theta_1);
	    double t3 = sin(theta_3);
	    double t4 = cos(theta_3);
	    double t5 = sin(theta_1);
	    double t6 = sin(theta_2);
	    double t7 = cos(theta_2);
	    double t8 = t4*t5;
	    double t9 = t2*t3*t6;
	    double t10 = t8+t9;
	    double t11 = t2*t4;
	    double t12 = t11-t3*t5*t6;
	    double t13 = t6*vector_1;
	    double t14 = t2*t7*vector_3;
	    double t15 = t13+t14-t5*t7*vector_2;
	    double t16 = t2*t3;
	    double t17 = t4*t5*t6;
	    double t18 = t16+t17;
	    double t19 = t3*t5;
	    double t20 = t19-t2*t4*t6;
	    A0(0,0) = t18*vector_3-t20*vector_2;
	    A0(0,1) = -t4*t15;
	    A0(0,2) = t10*vector_3+t12*vector_2-t3*t7*vector_1;
	    A0(1,0) = -t10*vector_2+t12*vector_3;
	    A0(1,1) = t3*t15;
	    A0(1,2) = -t18*vector_2-t20*vector_3-t4*t7*vector_1;
	    A0(2,0) = -t7*(t2*vector_2+t5*vector_3);
	    A0(2,1) = t7*vector_1-t2*t6*vector_3+t5*t6*vector_2;

	    return A0;
	}

	/**
	 * to map local angular velocity \omega_W expressed in body coordinates, to changes in Euler Angles expressed in an inertial frame q_I
	 * we have to map them via \dot{q}_I = H \omega_W, where H is the matrix defined in kindr getMappingFromLocalAngularVelocityToDiff.
	 * You can see the kindr cheat sheet to figure out how to build this matrix. The following code computes the Jacobian of \dot{q}_I
	 * with respect to \q_I and \omega_W. Thus the lower part of the Jacobian is H and the upper part is dH/dq_I \omega_W. We include
	 * both parts for more efficient computation. The following code is computed using auto-diff.
	 * @param eulerAnglesXyz
	 * @param angularVelocity
	 * @return
	 */
	Eigen::Matrix<double, 6, 3> jacobianOfAngularVelocityMapping(const Eigen::Matrix<double, 3, 1>& eulerAnglesXyz, const Eigen::Matrix<double, 3, 1>& angularVelocity)
	{
		using namespace std;

		Eigen::Matrix<double, 6, 1> xAD;
		xAD << eulerAnglesXyz, angularVelocity;
		const double* x = xAD.data();

		std::array<double, 10> v;

		Eigen::Matrix<double, 6, 3> jac;
		double* y = jac.data();

		y[9] = sin(x[2]);
		y[10] = cos(x[2]);
		v[0] = cos(x[1]);
		v[1] = 1 / v[0];
		y[3] = v[1] * y[10];
		v[2] = sin(x[1]);
		y[1] = 0 - (0 - (0 - x[4] * y[9] + x[3] * y[10]) * 1 / v[0] * v[1]) * v[2];
		v[3] = sin(x[2]);
		v[4] = 0 - v[1];
		y[4] = v[4] * y[9];
		v[5] = y[10];
		y[2] = 0 - x[3] * v[1] * v[3] + x[4] * v[4] * v[5];
		y[8] = 0 - x[4] * v[3] + x[3] * v[5];
		v[6] = v[1] * y[9];
		v[7] = v[4] * y[10];
		v[8] = v[2];
		y[15] = v[7] * v[8];
		y[16] = v[6] * v[8];
		v[9] = x[4] * v[8];
		v[8] = x[3] * v[8];
		y[13] = (x[4] * v[6] + x[3] * v[7]) * v[0] - (0 - (v[9] * y[9] - v[8] * y[10]) * 1 / v[0] * v[1]) * v[2];
		y[14] = 0 - v[8] * v[4] * v[3] + v[9] * v[1] * v[5];
		// dependent variables without operations
		y[0] = 0;
		y[5] = 0;
		y[6] = 0;
		y[7] = 0;
		y[11] = 0;
		y[12] = 0;
		y[17] = 1;


		return jac;
	}
};


}	// rbd
}	// ct


#endif  // RBDLINEARIZER_H_



#endif /* INCLUDE_CT_RBD_SYSTEMS_LINEAR_RBDLINEARIZER_H_ */