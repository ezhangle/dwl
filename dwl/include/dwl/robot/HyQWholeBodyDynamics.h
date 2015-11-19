#ifndef DWL__ROBOT__HYQ_WHOLE_BODY_DYNAMICS__H
#define DWL__ROBOT__HYQ_WHOLE_BODY_DYNAMICS__H

#include <dwl/model/RobCoGenWholeBodyDynamics.h>
#include <iit/robots/hyq/inverse_dynamics.h>
#include <iit/robots/hyq/inertia_properties.h>
#include <iit/robots/hyq/transforms.h>
//#include <iit/robots/hyq/jacobians.h>
//#include <iit/robots/hyq/transforms.h>


namespace dwl
{

namespace robot
{

class HyQWholeBodyDynamics : public model::RobCoGenWholeBodyDynamics
{
	public:
		HyQWholeBodyDynamics();
		~HyQWholeBodyDynamics();


	private:
		iit::HyQ::MotionTransforms motion_tf_;
		iit::HyQ::dyn::InertiaProperties inertia_;
		iit::HyQ::dyn::InverseDynamics id_;//(inertia_, motion_tf_);
};

} //@namespace model
} //@namespace dwl

#endif