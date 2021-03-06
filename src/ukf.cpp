#include "ukf.h"
#include "Eigen/Dense"
#include <fstream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  is_initialized_ = false;

  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;
 
  n_x_ = 5;

  n_aug_ = 7;

  n_sig_ = 2*n_aug_ + 1;

  // initial state vector
  x_ = VectorXd(n_x_);

  // initial covariance matrix
  P_ = MatrixXd::Identity(n_x_, n_x_);

  lambda_ = 3-n_x_;

  weights_ = VectorXd(n_sig_);
  // predict state mean vector
  weights_(0) = lambda_ / (lambda_+n_aug_);
  for (int i=1;i<n_sig_;i++)
    weights_(i) = 1 / (2*(lambda_+n_aug_));

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 1.41;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = .5;
  
  /**
   * DO NOT MODIFY measurement noise values below.
   * These are provided by the sensor manufacturer.
   */

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;
  
  /**
   * End DO NOT MODIFY section for measurement noise values 
   */
  
  /**
   * TODO: Complete the initialization. See ukf.h for other member properties.
   * Hint: one or more values initialized above might be wildly off...
   */

  R_l_ = MatrixXd(2,2);
  R_l_ << std_laspx_*std_laspx_, 0,
       0, std_laspy_*std_laspy_;
  

  R_r_ = MatrixXd(3,3);
  R_r_ << std_radr_*std_radr_, 0, 0,
       0, std_radphi_*std_radphi_, 0,
       0, 0, std_radrd_*std_radrd_;

}

UKF::~UKF() {}

void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Make sure you switch between lidar and radar
   * measurements.
   */

  if (!is_initialized_)
  {
    if (meas_package.sensor_type_ == meas_package.LASER)
    {
      x_ << 
        meas_package.raw_measurements_[0], 
        meas_package.raw_measurements_[1], 0, 0, 0;

    }
    else if (meas_package.sensor_type_ == meas_package.RADAR)
    {
      double r = meas_package.raw_measurements_[0];
      double p = meas_package.raw_measurements_[1];
      double rd = meas_package.raw_measurements_[2];
      double vx = rd * cos(p);
      double vy = rd * sin(p);

      x_ << 
        r * cos(p), 
        r * sin(p), 
        sqrt(vx*vx + vy*vy), 0, 0; // 

    };
    
    time_us_ = meas_package.timestamp_;

    is_initialized_ = true;
    return;
  }

  //if (meas_package.timestamp_ - time_us_ != 0)
  double dt = (meas_package.timestamp_-time_us_) / 1000000.0;
  time_us_ = meas_package.timestamp_;

  Prediction(dt);

  if (meas_package.sensor_type_ == MeasurementPackage::LASER && use_laser_)
    UpdateLidar(meas_package);
  if (meas_package.sensor_type_ == MeasurementPackage::RADAR && use_radar_)
    UpdateRadar(meas_package);

}

void UKF::Prediction(double delta_t) {
  /**
   * TODO: Complete this function! Estimate the object's location. 
   * Modify the state vector, x_. Predict sigma points, the state, 
   * and the state covariance matrix.
   */

  VectorXd x_aug = VectorXd(n_aug_);
  MatrixXd P_aug = MatrixXd(n_aug_,n_aug_); 
  MatrixXd Xsig_aug = MatrixXd(n_aug_, n_sig_);

  x_aug.fill(0.0);
  x_aug.head(n_x_) = x_;

  P_aug.fill(0.0);
  P_aug.topLeftCorner(n_x_,n_x_) = P_;
  P_aug(n_x_,n_x_) = std_a_*std_a_;
  P_aug(n_x_+1,n_x_+1) = std_yawdd_*std_yawdd_;

  MatrixXd A = P_aug.llt().matrixL();

  Xsig_aug.col(0) = x_aug;
  for (int i=0;i<n_aug_;i++)
  {
    Xsig_aug.col(i+1) = x_aug+sqrt(lambda_+n_aug_)*A.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug-sqrt(lambda_+n_aug_)*A.col(i);  
  }

  // predict sigma points
  Xsig_pred_ = MatrixXd(n_x_, n_sig_);
  for (int i=0;i<n_sig_;i++)
  {
    double px       = Xsig_aug(0,i);
    double py       = Xsig_aug(1,i);
    double v        = Xsig_aug(2,i);
    double psi      = Xsig_aug(3,i);
    double psiDot   = Xsig_aug(4,i);
    double nu_a     = Xsig_aug(5,i);
    double nu_psiDD = Xsig_aug(6,i);

    if (fabs(psiDot) > 0.001)
    {
      Xsig_pred_(0,i) = px + ((v/psiDot)*(sin(psi+psiDot*delta_t)-sin(psi))) + 0.5*delta_t*delta_t*cos(psi)*nu_a;
      Xsig_pred_(1,i) = py + ((v/psiDot)*(-cos(psi+psiDot*delta_t)+cos(psi))) + 0.5*delta_t*delta_t*sin(psi)*nu_a;
    }else
    {
      Xsig_pred_(0,i) = px + (v*cos(psi)*delta_t) + 0.5*delta_t*delta_t*cos(psi)*nu_a;
      Xsig_pred_(1,i) = py + (v*sin(psi)*delta_t) + 0.5*delta_t*delta_t*sin(psi)*nu_a;
    }
    
    Xsig_pred_(2,i) = v + delta_t*nu_a;
    Xsig_pred_(3,i) = psi + psiDot*delta_t + 0.5*delta_t*delta_t*nu_psiDD;
    Xsig_pred_(4,i) = psiDot + delta_t*nu_psiDD;
  }
  
  // predict state mean vector
  x_.fill(0.0);
  for (int i=0;i<n_sig_;i++)
    x_ += (weights_(i) * Xsig_pred_.col(i));
  
  // predict state mean covariance matrix
  P_.fill(0.0);
  for (int i=0;i<n_sig_;i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    while(x_diff(3)>M_PI) x_diff(3)-=2.*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI; 
    P_ += weights_(i)*x_diff*x_diff.transpose();
  }

}

void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use lidar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the lidar NIS, if desired.
   */
  int n_z = 2;

  MatrixXd Zsig = MatrixXd(n_z, n_sig_);
  VectorXd z_pred = VectorXd(n_z);
  MatrixXd S = MatrixXd(n_z,n_z);

  for (int i=0;i<n_sig_;i++)
  {
    Zsig(0,i) = Xsig_pred_(0,i);
    Zsig(1,i) = Xsig_pred_(1,i);
 }

  z_pred.fill(0.0);
  for (int i=0;i<n_sig_;i++)
    z_pred += weights_(i)*Zsig.col(i);

  S.fill(0.0);
  for (int i=0;i<n_sig_;i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    S += (weights_(i)*z_diff*z_diff.transpose());
  }
  S += R_l_;

  VectorXd z = VectorXd(n_z);
  z = meas_package.raw_measurements_;

  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for (int i=0;i<n_sig_;i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    Tc += weights_(i)*x_diff*z_diff.transpose();
  }

  MatrixXd K = MatrixXd(n_x_,n_z);
  K = Tc*S.inverse();

  // state vector
  VectorXd z_diff = z - z_pred;
  NIS_l_ = z_diff.transpose()*S.inverse()*z_diff;

  x_ += K*z_diff;
  // state covariance matrix
  P_ -= K*S*K.transpose();
}

void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
   * TODO: Complete this function! Use radar data to update the belief 
   * about the object's position. Modify the state vector, x_, and 
   * covariance, P_.
   * You can also calculate the radar NIS, if desired.
   */
  int n_z = 3;

  MatrixXd Zsig = MatrixXd(n_z, n_sig_);
  VectorXd z_pred = VectorXd(n_z);
  MatrixXd S = MatrixXd(n_z,n_z);

  for (int i=0;i<n_sig_;i++)
  {
    Zsig(0,i) = sqrt(Xsig_pred_(0,i)*Xsig_pred_(0,i) + Xsig_pred_(1,i)*Xsig_pred_(1,i));
    Zsig(1,i) = atan2(Xsig_pred_(1,i), Xsig_pred_(0,i));
    Zsig(2,i) = ((Xsig_pred_(0,i)*cos(Xsig_pred_(3,i))*Xsig_pred_(2,i)) + (Xsig_pred_(1,i)*sin(Xsig_pred_(3,i))*Xsig_pred_(2,i))) / Zsig(0,i);
  }

  z_pred.fill(0.0);
  for (int i=0;i<n_sig_;i++)
    z_pred += weights_(i)*Zsig.col(i);
  
  S.fill(0.0);
  for (int i=0;i<n_sig_;i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    while(z_diff(1)>M_PI) z_diff(1)-=2.*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI; 

    S += (weights_(i)*z_diff*z_diff.transpose());
  }
  S += R_r_;

  VectorXd z = VectorXd(n_z);
  z = meas_package.raw_measurements_;

  MatrixXd Tc = MatrixXd(n_x_,n_z);
  Tc.fill(0.0);
  for (int i=0;i<n_sig_;i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    while(z_diff(1)>M_PI) z_diff(1)-=2.*M_PI;
    while(z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI; 

    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    while(x_diff(3)>M_PI) x_diff(3)-=2.*M_PI;
    while(x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI; 

    Tc += weights_(i)*x_diff*z_diff.transpose();
  }

  MatrixXd K = MatrixXd(n_x_,n_z);
  K = Tc*S.inverse();

  // state vector
  VectorXd z_diff = z - z_pred;
  while(z_diff(1)>M_PI) z_diff(1)-=2.*M_PI;
  while(z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI; 

  NIS_r_ = z_diff.transpose()*S.inverse()*z_diff;

  x_ += K*z_diff;
  // state covariance matrix
  P_ -= K*S*K.transpose();
}