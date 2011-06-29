#ifndef INC_REMDTRAJ_H
#define INC_REMDTRAJ_H
#include "TrajectoryIO.h"
/// Class: RemdTraj
/// Used to read in replica trajectories. Hold each replica trajectory
/// in its own TrajectoryIO. During reads, only the frame with a temperature
/// matching remdtrajtemp will be used.
#include <vector>
class RemdTraj : public TrajectoryIO {
    // Private vars
    char *Prefix; // Complete filename to lowest replica up to the numerical extension
    int ExtWidth; // Size of the numerical extension in characters
    char *CompressExt; // If replica is compressed, hold the compression extension
    char *repFilename; // Will hold replica filename last set by GetReplicaName
    int lowestRepnum; // Hold the lowest repluica number
    bool hasTrajout; // True if writing replica trajectories during read
    double *remdX; // Hold coords of replica traj for writing out
    double remdbox[6];
    double remdT;
    int remdN;

    // Inherited functions
    int openTraj();
    void closeTraj();
    int readFrame(int,double*,double*,double*);
    //int writeFrame(int,double*,double*,double);
    void info();

  public:
    RemdTraj();
    ~RemdTraj();
    
    // RemdTraj-specific variables 
    double remdtrajtemp; // Get frames with this temperature on read
    std::vector<TrajectoryIO*> REMDtraj; // Input replica trajectories
    std::vector<TrajectoryIO*> REMDtrajout; // Output replica trajectories
    double *TemperatureList; // List of temperatures found in replicas
    
    // RemdTraj-specific functions
    int SetupTemperatureList(int);
    int SetReplicaName(char*);
    char *GetReplicaName(int);
    char *GetLowestReplicaName();
};
#endif
