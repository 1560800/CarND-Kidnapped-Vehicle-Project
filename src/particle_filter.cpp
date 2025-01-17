/**
 * particle_filter.cpp
 *
 * Created on: Dec 12, 2016
 * Author: Tiffany Huang
 */

#include "particle_filter.h"

#include <math.h>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include "helper_functions.h"

using std::string;
using std::vector;
using namespace std;

void ParticleFilter::init(double x, double y, double theta, double std[]) {
  /**
   * TODO: Set the number of particles. Initialize all particles to 
   *   first position (based on estimates of x, y, theta and their uncertainties
   *   from GPS) and all weights to 1. 
   * TODO: Add random Gaussian noise to each particle.
   * NOTE: Consult particle_filter.h for more information about this method 
   *   (and others in this file).
   */
  num_particles = 100;  // TODO: Set the number of particles
  default_random_engine gen;

  // This line creates a normal (Gaussian) distribution for x,y and theta
  normal_distribution<double> dist_x(0, std[0]);
  normal_distribution<double> dist_y(0, std[1]);
  normal_distribution<double> dist_theta(0, std[2]);

  for (int i = 0; i < num_particles; i++) {

	  Particle p;
	  p.id = i; // particles id
	  p.x = x + dist_x(gen); // x + x_noise;
	  p.y = y + dist_y(gen); // y + y_noise;
	  p.theta = theta + dist_theta(gen); // theta + theta_noise;
	  p.weight = 1.0; //all weights to 1
	  particles.push_back(p);
  }
  is_initialized = true;
}

void ParticleFilter::prediction(double delta_t, double std_pos[], 
                                double velocity, double yaw_rate) {
  /**
   * TODO: Add measurements to each particle and add random Gaussian noise.
   * NOTE: When adding noise you may find std::normal_distribution 
   *   and std::default_random_engine useful.
   *  http://en.cppreference.com/w/cpp/numeric/random/normal_distribution
   *  http://www.cplusplus.com/reference/random/default_random_engine/
   */
	default_random_engine gen;
	normal_distribution<double> dist_x(0, std_pos[0]);
	normal_distribution<double> dist_y(0, std_pos[1]);
	normal_distribution<double> dist_theta(0, std_pos[2]);

	for (int i = 0; i < num_particles; i++) {
		if (fabs(yaw_rate) > 0.001) {
			particles[i].x += (velocity / yaw_rate) * (sin(particles[i].theta + (yaw_rate * delta_t)) - sin(particles[i].theta));
			particles[i].y += (velocity / yaw_rate) * (-cos(particles[i].theta + (yaw_rate * delta_t)) + cos(particles[i].theta));
			particles[i].theta += yaw_rate * delta_t;
		}else {
			particles[i].x += velocity * delta_t * cos(particles[i].theta);
			particles[i].y += velocity * delta_t * sin(particles[i].theta);
		}
		particles[i].x += dist_x(gen); // x + x_noise;
		particles[i].y += dist_y(gen); // y + y_noise;
		particles[i].theta += dist_theta(gen); // theta + theta_noise;
	}
}

void ParticleFilter::dataAssociation(vector<LandmarkObs> predicted, 
                                     vector<LandmarkObs>& observations) {
  /**
   * TODO: Find the predicted measurement that is closest to each 
   *   observed measurement and assign the observed measurement to this 
   *   particular landmark.
   * NOTE: this method will NOT be called by the grading code. But you will 
   *   probably find it useful to implement this method and use it as a helper 
   *   during the updateWeights phase.
   */
	for (unsigned int i = 0 ; i < observations.size(); i++) {
		double min_dist = numeric_limits<double>::max();
		int map_id = -1;

		for (unsigned int j = 0; j < predicted.size(); j++) {
			double current_dist = dist(observations[i].x, observations[i].y, predicted[j].x, predicted[j].y);

			if (min_dist > current_dist) {
				min_dist = current_dist;
				map_id = predicted[j].id;
			}
		}
		observations[i].id = map_id;
	}

}

void ParticleFilter::updateWeights(double sensor_range, double std_landmark[], 
                                   const vector<LandmarkObs> &observations, 
                                   const Map &map_landmarks) {
  /**
   * TODO: Update the weights of each particle using a mult-variate Gaussian 
   *   distribution. You can read more about this distribution here: 
   *   https://en.wikipedia.org/wiki/Multivariate_normal_distribution
   * NOTE: The observations are given in the VEHICLE'S coordinate system. 
   *   Your particles are located according to the MAP'S coordinate system. 
   *   You will need to transform between the two systems. Keep in mind that
   *   this transformation requires both rotation AND translation (but no scaling).
   *   The following is a good resource for the theory:
   *   https://www.willamette.edu/~gorr/classes/GeneralGraphics/Transforms/transforms2d.htm
   *   and the following is a good resource for the actual equation to implement
   *   (look at equation 3.33) http://planning.cs.uiuc.edu/node99.html
   */
	for (int i = 0; i < num_particles; i++) {
		//Predicted landmarks with sensor range
		vector <LandmarkObs>  predicted;
		for (unsigned int j = 0; j < map_landmarks.landmark_list.size(); j++) {
			int landmark_id = map_landmarks.landmark_list[j].id_i;
			float landmark_x = map_landmarks.landmark_list[j].x_f;
			float landmark_y = map_landmarks.landmark_list[j].y_f;
			if (dist(particles[i].x , particles[i].y , landmark_x , landmark_y) < sensor_range) {
				predicted.push_back(LandmarkObs{ landmark_id,landmark_x, landmark_y});
			}
		}
		// Transforming observations into global coordinate system.
		vector <LandmarkObs> transformed_Obs;
		for (unsigned int j = 0; j < observations.size(); j++) {
			double trans_x = particles[i].x + (cos(particles[i].theta) * observations[j].x) - (sin(particles[i].theta) * observations[j].y);
			double trans_y = particles[i].y + (sin(particles[i].theta) * observations[j].x) + (cos(particles[i].theta) * observations[j].y);
			transformed_Obs.push_back(LandmarkObs{ observations[j].id, trans_x, trans_y });
		}
		// Match subset of landmarks to their observations.
		dataAssociation(predicted, transformed_Obs);

		// Initialize particle weight.
		particles[i].weight = 1.0;

		for (unsigned int j = 0; j < transformed_Obs.size(); j++) {
			int index = 0;
			while (transformed_Obs[j].id != predicted[index].id) {
				++index;
			}
			// Caluclate the weight of each particle by using the multivate Gaussian probability density function.
			particles[i].weight *= multiv_prob(std_landmark[0], std_landmark[1], transformed_Obs[j].x, transformed_Obs[j].y,
				predicted[index].x, predicted[index].y);
		}
	}
}

void ParticleFilter::resample() {
  /**
   * TODO: Resample particles with replacement with probability proportional 
   *   to their weight. 
   * NOTE: You may find std::discrete_distribution helpful here.
   *   http://en.cppreference.com/w/cpp/numeric/random/discrete_distribution
   */

	// Get an array of weights for a discrete distribution
	vector<double> weights;
	for (int i = 0; i < num_particles; i++) {
		weights.push_back(particles[i].weight);
	}
	
	default_random_engine gen;
	// Resampling
	vector<Particle> new_particles;

	for (int i = 0; i < num_particles; i++) {
		discrete_distribution<> dst2(weights.begin(), weights.end());
		new_particles.push_back(particles[dst2(gen)]);
	}
	particles = new_particles;
}

void ParticleFilter::SetAssociations(Particle& particle, 
                                     const vector<int>& associations, 
                                     const vector<double>& sense_x, 
                                     const vector<double>& sense_y) {
  // particle: the particle to which assign each listed association, 
  //   and association's (x,y) world coordinates mapping
  // associations: The landmark id that goes along with each listed association
  // sense_x: the associations x mapping already converted to world coordinates
  // sense_y: the associations y mapping already converted to world coordinates
  particle.associations= associations;
  particle.sense_x = sense_x;
  particle.sense_y = sense_y;
}

string ParticleFilter::getAssociations(Particle best) {
  vector<int> v = best.associations;
  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<int>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}

string ParticleFilter::getSenseCoord(Particle best, string coord) {
  vector<double> v;

  if (coord == "X") {
    v = best.sense_x;
  } else {
    v = best.sense_y;
  }

  std::stringstream ss;
  copy(v.begin(), v.end(), std::ostream_iterator<float>(ss, " "));
  string s = ss.str();
  s = s.substr(0, s.length()-1);  // get rid of the trailing space
  return s;
}