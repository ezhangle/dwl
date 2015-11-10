#include <dwl/locomotion/WholeBodyTrajectoryOptimization.h>


namespace dwl
{

namespace locomotion
{

WholeBodyTrajectoryOptimization::WholeBodyTrajectoryOptimization() : solver_(NULL)
{

}


WholeBodyTrajectoryOptimization::~WholeBodyTrajectoryOptimization()
{

}

void WholeBodyTrajectoryOptimization::init(solver::OptimizationSolver* solver)
{
	solver_ = solver;
	solver_->init();
}


void WholeBodyTrajectoryOptimization::addDynamicalSystem(model::DynamicalSystem* system)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().addDynamicalSystem(system);
	else
		printf(RED "FATAL: there was not defined a solver for setting the %s dynamical constraint"
				COLOR_RESET, system->getName().c_str());
}


void WholeBodyTrajectoryOptimization::removeDynamicalSystem()
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().removeDynamicalSystem();
	else
		printf(YELLOW "WARNING: there was not defined a solver, it could be removed the dynamical"
				" constraint");
}


void WholeBodyTrajectoryOptimization::addConstraint(model::Constraint* constraint)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().addConstraint(constraint);
	else
		printf(RED "FATAL: there was not defined a solver for setting the %s constraint"
				COLOR_RESET, constraint->getName().c_str());
}


void WholeBodyTrajectoryOptimization::removeConstraint(std::string name)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().removeConstraint(name);
	else
		printf(YELLOW "WARNING: there was not defined a solver, it could be removed the constraint");
}


void WholeBodyTrajectoryOptimization::addCost(model::Cost* cost)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().addCost(cost);
	else
		printf(RED "FATAL: there is not defined a solver for setting the %s cost"
				COLOR_RESET, cost->getName().c_str());
}


void WholeBodyTrajectoryOptimization::removeCost(std::string name)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().removeCost(name);
	else
		printf(YELLOW "WARNING: there was not defined a solver, it could be removed the cost");
}


void WholeBodyTrajectoryOptimization::setHorizon(unsigned int horizon)
{
	if (solver_ != NULL)
		solver_->getOptimizationModel().setHorizon(horizon);
	else
		printf(RED "FATAL: there is not defined a solver for setting the horizon" COLOR_RESET);
}


void WholeBodyTrajectoryOptimization::setStepIntegrationTime(const double& step_time)
{
	solver_->getOptimizationModel().getDynamicalSystem()->setStepIntegrationTime(step_time);
}


void WholeBodyTrajectoryOptimization::setNominalTrajectory(std::vector<WholeBodyState>& nom_trajectory)
{
	solver_->getOptimizationModel().setStartingTrajectory(nom_trajectory);
}


bool WholeBodyTrajectoryOptimization::compute(const WholeBodyState& current_state,
											  const WholeBodyState& desired_state,
											  double computation_time)
{
	if (solver_ != NULL) {
		// Setting the current state, terminal and the starting state for the optimization
		solver_->getOptimizationModel().getDynamicalSystem()->setInitialState(current_state);
		solver_->getOptimizationModel().getDynamicalSystem()->setTerminalState(desired_state);

		// Setting the desired state to the cost functions
		unsigned int num_cost = solver_->getOptimizationModel().getCosts().size();
		for (unsigned int i = 0; i < num_cost; i++)
			solver_->getOptimizationModel().getCosts()[i]->setDesiredState(desired_state);

		return solver_->compute(computation_time);
	} else {
		printf(RED "FATAL: there was not defined a solver" COLOR_RESET);
		return false;
	}
}


model::DynamicalSystem* WholeBodyTrajectoryOptimization::getDynamicalSystem()
{
	return solver_->getOptimizationModel().getDynamicalSystem();
}


const WholeBodyTrajectory& WholeBodyTrajectoryOptimization::getWholeBodyTrajectory()
{
	return solver_->getWholeBodyTrajectory();
}


const WholeBodyTrajectory& WholeBodyTrajectoryOptimization::getInterpolatedWholeBodyTrajectory(const double& interpolation_time)
{
	// Deleting old information
	interpolated_trajectory_.clear();

	// Getting the whole-body trajectory
	WholeBodyTrajectory trajectory = solver_->getWholeBodyTrajectory();

	// Getting the number of joints and end-effectors
	unsigned int num_joints = getDynamicalSystem()->getFloatingBaseSystem().getJointDoF();
	unsigned int num_contacts = getDynamicalSystem()->getFloatingBaseSystem().getNumberOfEndEffectors();

	// Defining splines //TODO for the time being only cubic interpolation is OK
	std::vector<math::CubicSpline> base_spline, joint_spline, control_spline;
	std::vector<std::vector<math::CubicSpline> > contact_force_spline;
	base_spline.resize(6);
	joint_spline.resize(num_joints);
	control_spline.resize(num_joints);
	contact_force_spline.resize(num_contacts);

	// Computing the interpolation of the whole-body trajectory
	unsigned int horizon = solver_->getOptimizationModel().getHorizon();
	for (unsigned int k = 0; k < horizon; k++) {
		// Adding the starting state
		interpolated_trajectory_.push_back(trajectory[k]);

		// Getting the current starting times
		double starting_time = trajectory[k].time;
		double duration = trajectory[k+1].duration;

		// Interpolating the current state
		WholeBodyState current_state(num_joints);
		math::Spline::Point current_point;
		unsigned int index = floor(duration / interpolation_time);
		for (unsigned int t = 0; t < index; t++) {
			double time = starting_time + t * interpolation_time;

			// Base interpolation
			for (unsigned int base_idx = 0; base_idx < 6; base_idx++) {
				if (t == 0) {
					// Initialization of the base motion splines
					math::Spline::Point starting(trajectory[k].base_pos(base_idx),
											 	 trajectory[k].base_vel(base_idx),
												 trajectory[k].base_acc(base_idx));
					math::Spline::Point ending(trajectory[k+1].base_pos(base_idx),
											   trajectory[k+1].base_vel(base_idx),
											   trajectory[k+1].base_acc(base_idx));
					base_spline[base_idx].setBoundary(starting_time, duration, starting, ending);
				} else {
					// Getting and setting the interpolated point
					base_spline[base_idx].getPoint(time, current_point);
					current_state.base_pos(base_idx) = current_point.x;
					current_state.base_vel(base_idx) = current_point.xd;
					current_state.base_acc(base_idx) = current_point.xdd;
				}
			}

			// Joint interpolations
			for (unsigned int joint_idx = 0; joint_idx < num_joints; joint_idx++) {
				if (t == 0) {
					// Initialization of the joint motion splines
					math::Spline::Point motion_starting(trajectory[k].joint_pos(joint_idx),
														trajectory[k].joint_vel(joint_idx),
														trajectory[k].joint_acc(joint_idx));
					math::Spline::Point motion_ending(trajectory[k+1].joint_pos(joint_idx),
													  trajectory[k+1].joint_vel(joint_idx),
													  trajectory[k+1].joint_acc(joint_idx));
					joint_spline[joint_idx].setBoundary(starting_time, duration,
														motion_starting, motion_ending);

					// Initialization of the joint control splines
					math::Spline::Point control_starting(trajectory[k].joint_eff(joint_idx));
					math::Spline::Point control_ending(trajectory[k+1].joint_eff(joint_idx));
					control_spline[joint_idx].setBoundary(starting_time, duration,
														  control_starting, control_ending);
				} else {
					// Getting and setting the joint motion interpolated point
					joint_spline[joint_idx].getPoint(time, current_point);
					current_state.joint_pos(joint_idx) = current_point.x;
					current_state.joint_vel(joint_idx) = current_point.xd;
					current_state.joint_acc(joint_idx) = current_point.xdd;

					// Getting and setting the joint motion interpolated point
					control_spline[joint_idx].getPoint(time, current_point);
					current_state.joint_eff(joint_idx) = current_point.x;
				}
			}

			// Contact interpolation if there are part of the optimization variables
			if (trajectory[k].contact_pos.size() != 0) {
				urdf_model::LinkID contact_links = getDynamicalSystem()->getFloatingBaseSystem().getEndEffectors();
				for (urdf_model::LinkID::const_iterator contact_it = contact_links.begin();
						contact_it != contact_links.end(); contact_it++) {
					std::string name = contact_it->first;
					unsigned int id = contact_it->second;

					// Sanity check: checking if there are effort information
					rbd::BodyWrench::const_iterator init_it = trajectory[k].contact_eff.find(name);
					rbd::BodyWrench::const_iterator end_it = trajectory[k+1].contact_eff.find(name);
					if (init_it != trajectory[k].contact_eff.end() &&
							end_it != trajectory[k+1].contact_eff.end()) {
						// Resizing the number of coordinate of the spline
						contact_force_spline[id].resize(3);

						rbd::Vector6d init_eff = init_it->second;
						rbd::Vector6d end_eff = end_it->second;

						// Computing the contact interpolation per each coordinate (x,y,z)
						rbd::Vector6d eff_state = rbd::Vector6d::Zero();
						for (unsigned int coord_idx = 3; coord_idx < 6; coord_idx++) {
							// Getting the 6d coordinate
							rbd::Coords6d coord = rbd::Coords6d(coord_idx);
							if (t == 0) {
								// Initialization of the contact force splines
								math::Spline::Point force_starting(init_eff(coord));
								math::Spline::Point force_ending(end_eff(coord));
								contact_force_spline[id][coord_idx].setBoundary(starting_time, duration,
																				force_starting, force_ending);
							} else {
								// Getting and setting the force interpolated point
								contact_force_spline[id][coord_idx-3].getPoint(time, current_point);
								eff_state(coord) = current_point.x;
							}
						}
						current_state.contact_eff[name] = eff_state;
					}
				}
			}

			// Adding the current state
			if (t != 0) {
				// Setting the current time
				current_state.time = time;

				interpolated_trajectory_.push_back(current_state);
			}
		}
	}

	// Adding the ending state
	interpolated_trajectory_.push_back(trajectory[horizon]);

	return interpolated_trajectory_;
}


} //@namespace locomotion
} //@namespace dwl
