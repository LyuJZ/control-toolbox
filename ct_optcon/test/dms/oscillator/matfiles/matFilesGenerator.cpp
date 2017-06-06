// #define MATLAB

/*
 * TODO: FIXME -- bring back Matlab interface
 * */

#include <cstring>
#include <iostream>
#include <memory>

#include <ct/optcon/optcon.h>

#include "oscDMSTest_settings.h"

//#include <ct/optcon/costfunction/CostFunctionQuadraticSimple.hpp>
//
//#include <gtest/gtest.h>
//#include <ct/core/core.h>
//
//#include <ct/optcon/problem/OptConProblem.h>
//#include <ct/optcon/dms/dms_core/DmsSolver.h>
//#include <ct/optcon/dms/dms_core/DmsSettings.hpp>
//#include <ct/optcon/nlp/solver/NlpSolverSettings.h>
//
//#include "oscDMSTest_settings.h"
//
//#include <ct/optcon/dms/util/MatlabInterface.hpp>
//
//#include <ct/optcon/constraint/term/TerminalConstraint.h>
//#include <ct/optcon/constraint/ConstraintContainerAnalytical.h>


 using namespace ct;
using namespace optcon;


class MatFilesGenerator
{
public:
	typedef DmsDimensions<2,1> OscDimensions;

	MatFilesGenerator() :
	w_n_(0.5),
	zeta_(0.01)
	{
		matlabPathIPOPT_ = std::string(DATA_DIR) + "/solutionIpopt.mat";
		matlabPathSNOPT_ = std::string(DATA_DIR) + "/solutionSnopt.mat";
		settings_.N_ = 25;
		settings_.T_ = 5.0;
		settings_.nThreads_ = 2;
		settings_.terminalStateConstraint_ = 1;
		settings_.splineType_ = DmsSettings::PIECEWISE_LINEAR;
		settings_.costEvaluationType_ = DmsSettings::FULL;
		settings_.objectiveType_ = DmsSettings::OPTIMIZE_GRID;
		settings_.h_min_ = 0.1; // minimum admissible distance between two nodes in [sec]
		settings_.integrationType_ = DmsSettings::RK4;
		settings_.dt_sim_ = 0.01;
		settings_.integrateSens_ = 1;
		settings_.absErrTol_ = 1e-8;
		settings_.relErrTol_ = 1e-8;
	}

	void initialize()
	{
		oscillator_ = std::shared_ptr<ct::core::SecondOrderSystem> (new ct::core::SecondOrderSystem(w_n_, zeta_));
		x_0_ << 0.0,0.0;
		x_final_ << 2.0, -1.0;
		Q_ << 	0.0,0.0,
				0.0,10.0;

		Q_final_ << 0.0,0.0,
					0.0,0.0;

		R_ << 0.001;
		u_des_ << 0.0;

		costFunction_ = std::shared_ptr<ct::optcon::CostFunctionQuadratic<2,1>> 
				(new ct::optcon::CostFunctionQuadraticSimple<2,1>(Q_, R_, x_final_, u_des_, x_final_, Q_final_));
	}

	void generateMatFilesIPOPT()
	{
		settings_.nlpSettings_.solverType_ = NlpSolverSettings::IPOPT;

		finalConstraints_ = std::shared_ptr<ct::optcon::ConstraintContainerAnalytical<2, 1>>
				(new ct::optcon::ConstraintContainerAnalytical<2, 1>());

		std::shared_ptr<TerminalConstraint<2,1>> termConstraint(new TerminalConstraint<2,1>(x_final_));

		termConstraint->setName("crazyTerminalConstraint");
		finalConstraints_->addConstraint(termConstraint, true);
		finalConstraints_->initialize();

		OptConProblem<2,1> optProblem(oscillator_, costFunction_);
		optProblem.setInitialState(x_0_);

		optProblem.setTimeHorizon(settings_.T_);
		optProblem.setFinalConstraints(finalConstraints_);

		dmsPlanner_ = std::shared_ptr<DmsSolver<2,1>> (new DmsSolver<2,1>(optProblem, settings_));

		calcInitGuess();
		dmsPlanner_->setInitialGuess(initialPolicy_);
		dmsPlanner_->solve();
		solutionPolicy_ = dmsPlanner_->getSolution();

		//Solution Containers
		OscDimensions::state_vector_array_t stateSolutionIpopt;
		OscDimensions::control_vector_array_t inputSolutionIpopt;
		OscDimensions::time_array_t timeSolutionIpopt;
		stateSolutionIpopt = solutionPolicy_.xSolution_;
		inputSolutionIpopt = solutionPolicy_.uSolution_;
		timeSolutionIpopt = solutionPolicy_.tSolution_;


#ifdef MATLAB
		MatlabInterface mi(matlabPathIPOPT_);
		mi.sendMultiDimTrajectoryToMatlab<OscDimensions::state_vector_array_t>(stateSolutionIpopt, 2, "stateDmsIpopt");
		mi.sendMultiDimTrajectoryToMatlab<OscDimensions::control_vector_array_t>(inputSolutionIpopt, 1, "inputDmsIpopt");
		// mi.sendMultiDimTrajectoryToMatlab<OscDimensions::time_array_t>(timeSolutionIpopt, 1, "timeDmsIpopt");
		mi.sendScalarArrayToMatlab(timeSolutionIpopt, "timeDmsIpopt");
#endif //MATLAB

	}

	void generateMatFilesSNOPT()
	{
		settings_.nlpSettings_.solverType_ = NlpSolverSettings::SNOPT;

		finalConstraints_ = std::shared_ptr<ct::optcon::ConstraintContainerAnalytical<2, 1>>
				(new ct::optcon::ConstraintContainerAnalytical<2, 1>());

		std::shared_ptr<TerminalConstraint<2,1>> termConstraint(new TerminalConstraint<2,1>(x_final_));

		termConstraint->setName("crazyTerminalConstraint");
		finalConstraints_->addConstraint(termConstraint, true);

		OptConProblem<2,1> optProblem(oscillator_, costFunction_);
		optProblem.setInitialState(x_0_);

		optProblem.setTimeHorizon(settings_.T_);
		optProblem.setFinalConstraints(finalConstraints_);
		finalConstraints_->initialize();

		dmsPlanner_ = std::shared_ptr<DmsSolver<2,1>> (new DmsSolver<2,1>(optProblem, settings_));

		calcInitGuess();
		dmsPlanner_->setInitialGuess(initialPolicy_);
		dmsPlanner_->solve();
		solutionPolicy_ = dmsPlanner_->getSolution();

		//Solution Containers
		OscDimensions::state_vector_array_t stateSolutionSnopt;
		OscDimensions::control_vector_array_t inputSolutionSnopt;
		OscDimensions::time_array_t timeSolutionSnopt;
		stateSolutionSnopt = solutionPolicy_.xSolution_;
		inputSolutionSnopt = solutionPolicy_.uSolution_;
		timeSolutionSnopt = solutionPolicy_.tSolution_;

#ifdef MATLAB
		MatlabInterface mi(matlabPathSNOPT_);
		mi.sendMultiDimTrajectoryToMatlab<OscDimensions::state_vector_array_t>(stateSolutionSnopt, 2, "stateDmsSnopt");
		mi.sendMultiDimTrajectoryToMatlab<OscDimensions::control_vector_array_t>(inputSolutionSnopt, 1, "inputDmsSnopt");
		mi.sendScalarArrayToMatlab(timeSolutionSnopt, "timeDmsSnopt");
#endif //MATLAB
	}


private:
	void calcInitGuess()
	{
		x_initguess_.resize(settings_.N_ + 1, OscDimensions::state_vector_t::Zero());
		u_initguess_.resize(settings_.N_ + 1, OscDimensions::control_vector_t::Zero());
		for(size_t i = 0; i < settings_.N_ + 1; ++i)
		{
			x_initguess_[i] = x_0_ + (x_final_ - x_0_) * (i / settings_.N_);
		}

		initialPolicy_.xSolution_ = x_initguess_;
		initialPolicy_.uSolution_ = u_initguess_;
	}

	double w_n_;
	double zeta_;
	std::shared_ptr<ct::core::SecondOrderSystem > oscillator_;

	std::string matlabPathIPOPT_;
	std::string matlabPathSNOPT_;

	DmsSettings settings_;
	std::shared_ptr<DmsSolver<2, 1>> dmsPlanner_;
	std::shared_ptr<ct::optcon::CostFunctionQuadratic<2,1> >  costFunction_;
	std::shared_ptr<ct::optcon::ConstraintContainerAnalytical<2, 1> > finalConstraints_;


	DmsPolicy<2, 1> initialPolicy_;
	DmsPolicy<2, 1> solutionPolicy_;
	OscDimensions::state_vector_array_t x_initguess_;
	OscDimensions::control_vector_array_t u_initguess_;

	OscDimensions::state_vector_t x_0_;
	OscDimensions::state_vector_t x_final_;
	OscDimensions::state_matrix_t Q_;
	OscDimensions::state_matrix_t Q_final_;
	OscDimensions::control_matrix_t R_;
	OscDimensions::control_vector_t u_des_;
};


int main( int argc, char* argv[] )
{

	MatFilesGenerator oscDms;
	oscDms.initialize();
#ifdef BUILD_WITH_SNOPT_SUPPORT
	std::cout << "Generating Mat Files using SNOPT:" << std::endl;
	oscDms.generateMatFilesSNOPT();
#endif// BUILD_WITH_IPOPT_SUPPORT

#ifdef BUILD_WITH_IPOPT_SUPPORT
	std::cout << "Generating Mat Files using IPOPT:" << std::endl;
	oscDms.generateMatFilesIPOPT();
#endif // BUILD_WITH_IPOPT_SUPPORT

	return 0;
}