#include <cstdlib> // abs
#include <cmath> // sqrt
#include "Analysis_CrankShaft.h"
#include "CpptrajStdio.h"

Analysis_CrankShaft::Analysis_CrankShaft() :
  start_(0),
  stop_(-1),
  offset_(1),
  type_(ANGLE),
  angletype_(NOTYPE),
  scalar1_(0),
  scalar2_(0)
{}

const char Analysis_CrankShaft::CSstring[2][9] = {
  "angle", "distance"
};

/** analyze crankshaft {angle | distance} <scalar-name1> <scalar-name2> 
  *                     info <string>
  */
int Analysis_CrankShaft::Setup(DataSetList *DSLin) {
  // Ensure first 2 args (should be 'analyze' 'crank') are marked.
  analyzeArgs_.MarkArg(0);
  analyzeArgs_.MarkArg(1);

  info_ = analyzeArgs_.GetStringKey("info");
  if (info_.empty())
    info_.assign("");

  if (analyzeArgs_.hasKey("angle"))
    type_ = ANGLE;
  else if (analyzeArgs_.hasKey("distance"))
    type_ = DISTANCE;

  filename_ = analyzeArgs_.GetStringKey("out");

  start_ = analyzeArgs_.getKeyInt("start", 1);
  --start_;
  stop_ = analyzeArgs_.getKeyInt("stop", -1);
  offset_ = analyzeArgs_.getKeyInt("offset",1);

  // Get dataset names
  std::string name1 = analyzeArgs_.GetStringNext();
  if (name1.empty()) {
    mprinterr("Error: crankshaft: No name specified for dataset 1.\n");
    return 1;
  }
  std::string name2 = analyzeArgs_.GetStringNext();
  if (name2.empty()) {
    mprinterr("Error: crankshaft: No name specified for dataset 2.\n");
    return 1;
  }

  // Get datasets
  scalar1_ = DSLin->Get( name1.c_str() );
  if (scalar1_ == NULL) {
    mprinterr("Error: crankshaft: Dataset %s not found.\n", name1.c_str());
    return 1;
  }
  scalar2_ = DSLin->Get( name2.c_str() );
  if (scalar2_ == NULL) {
    mprinterr("Error: crankshaft: Dataset %s not found.\n", name2.c_str());
    return 1;
  } 

  // INFO:
  mprintf("    ANALYZE CRANKSHAFT: %s ", info_.c_str());
  mprintf("%ss named %s and %s\n", CSstring[type_], name1.c_str(), name2.c_str());
  mprintf("\tFrames %i to ", start_+1);
  if (stop_==-1)
    mprintf("last");
  else
    mprintf("%i", stop_);
  mprintf(", offset %i\n", offset_);

  return 0;
}

const double Analysis_CrankShaft::torsion_offset[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 180.0 };

const char  Analysis_CrankShaft::torsion_ss[6][8] = {
  "g+     ", "a+     ", "t      ", "a-     ", "g-     ", "c      "
};

const char  Analysis_CrankShaft::distance_ss_2D[6][6][9] = {
  {"< 2, < 2", "< 2, 2-3", "< 2, 3-4", "< 2, 4-5", "< 2, 5-6", "< 2, > 6" },
  {"2-3, < 2", "2-3, 2-3", "2-3, 3-4", "2-3, 4-5", "2-3, 5-6", "2-3, > 6" },
  {"3-4, < 2", "3-4, 2-3", "3-4, 3-4", "3-4, 4-5", "3-4, 5-6", "3-4, > 6" },
  {"4-5, < 2", "4-5, 2-3", "4-5, 3-4", "4-5, 4-5", "4-5, 5-6", "4-5, > 6" },
  {"5-6, < 2", "5-6, 2-3", "5-6, 3-4", "5-6, 4-5", "5-6, 5-6", "5-6, > 6" },
  {"> 6, < 2", "> 6, 2-3", "> 6, 3-4", "> 6, 4-5", "> 6, 5-6", "> 6, > 6" }
};

const char  Analysis_CrankShaft::torsion_ss_2D[6][6][6] = {
  {"g+ g+", "g+ a+", "g+ t", "g+ a-", "g+ g-", "g+ c"},
  {"a+ g+", "a+ a+", "a+ t", "a+ a-", "a+ g-", "a+ c"},
  {"t  g+", "t  a+", "t  t", "t  a-", "t  g-", "t  c"},
  {"a- g+", "a- a+", "a- t", "a- a-", "a- g-", "a- c" },
  {"g- g+", "g- a+", "g- t", "g- a-", "g- g-", "g- c" },
  {"c  g+", "c  a+", "c  t", "c  a-", "c  g-", "c  c" }
};

// Analysis_CrankShaft::Analyze()
int Analysis_CrankShaft::Analyze() {
  double v1_avg[6][6], v2_avg[6][6], v1_sd[6][6], v2_sd[6][6];
  int visits[6][6], transitions[6][6];

  // Check that scalar1 and scalar2 have same # data points.
  int Nelements = scalar1_->Size();
  if (Nelements != scalar2_->Size()) {
    mprinterr("Error: crankshaft: # elements in dataset %s (%i) not equal to\n",
              scalar1_->c_str(), Nelements);
    mprinterr("                   # elements in dataset %s (%i)\n",
              scalar2_->c_str(), scalar2_->Size());
    return 1;
  }
  if (stop_ == -1)
    stop_ = Nelements;
  if (start_ >= Nelements) {
    mprinterr("Error: crankshaft: start (%i) >= total # elements.\n",start_+1, Nelements);
    return 1;
  }
  int totalFrames = (stop_ - start_) / offset_;
  mprintf("\tcrankshaft: Processing %i frames.\n", totalFrames);

  // Initialization
  for (int j=0;j<6;j++) {
    for (int k=0;k<6;k++) {
      v1_avg[j][k] = 0.0;
      v2_avg[j][k] = 0.0;
      v1_sd[j][k] = 0.0;
      v2_sd[j][k] = 0.0;

      visits[j][k] = 0;
      transitions[j][k] = 0;
    }
  }

  // Open output file
  CpptrajFile outfile;
  if (outfile.OpenWrite( filename_ )) return 1;

  // MAIN LOOP over frames
  int i1 = 0;
  int i2 = 0;
  int initial_i1 = 0;
  int initial_i2 = 0;
  int previous_i1 = 0;
  int previous_i2 = 0;
  int final_i1 = 0;
  int final_i2 = 0;
  double initial_v1 = 0;
  double initial_v2 = 0;
  double final_v1 = 0;
  double final_v2 = 0;
  for (int frame = start_; frame < stop_; frame += offset_) {
    double v1 = scalar1_->Dval( frame );
    double v2 = scalar1_->Dval( frame );

    if ( type_ == DISTANCE ) {
      // This is a DISTANCE
      i1 = (v1 - 1.0) / 1;     // The current algorithm is a test and aims to bin things 
      if (i1 > 5) i1 = 5;      // from 0->5 starting from values < 2A, in increments of 1
      i2 = (v2 - 1.0) / 1;     // angstrom to > 6 A.  i.e. value-1.0/1                   
      if (i2 > 5) i2 = 5;
    } else { // if type_ == ANGLE
      // This is an ANGLE
      //   -- bin from 0->5
      //   -- subtract 30 from value, such that 0->60 = g+, 60->120 = a+, 120->180 = t

      v1 -= 30.0;
      v2 -= 30.0;

      if (v1 < 0)    v1 += 360.0;
      if (v1 > 360)  v1 -= 360.0;
      if (v2 < 0)    v2 += 360.0;
      if (v2 > 360)  v2 -= 360.0;

      i1 = v1 / 60;
      i2 = v2 / 60;
    }

    // Store initial and final bins/values
    if (frame == start_) {
      previous_i1 = initial_i1 = i1;
      previous_i2 = initial_i2 = i2;
      initial_v1 = scalar1_->Dval( frame );
      initial_v2 = scalar2_->Dval( frame );
    }
    final_i1 = i1;
    final_i2 = i2;
    final_v1 = scalar1_->Dval( frame );
    final_v2 = scalar2_->Dval( frame );

    // DEBUG
    if (debug_ > 1) {
      mprintf("Binning %s values %6.2f %6.2f into %i x %i\n",
              CSstring[type_], final_v1, final_v2, i1, i2); 
    }

    // update bin counter and averages/standard deviations
    ++visits[i1][i2];

    v1 = final_v1;
    v2 = final_v2;
    v1 += torsion_offset[i1];
    v2 += torsion_offset[i2];
    v1_avg[i1][i2] += v1;
    v2_avg[i1][i2] += v2;

    v1_sd[i1][i2]  = v1_sd[i1][i2] + (v1*v1);
    v2_sd[i1][i2]  = v2_sd[i1][i2] + (v2*v2);

    if (!filename_.empty()) {
      //  hack to map substate numbers 0->36 to a-z0-9
      //
      //  j = i1*6 + i2 + 97;
      //  if (j > 122)
      //  j -= 75;
      //
      //       g+    a+    t    a-    g-    c
      //  g+   0     1     2    3     4     5
      //  a+   6     7     8    9    10    11
      outfile.Printf("%7i %i\n", frame, i1*6 + i2);
    }

    //  check for transitions from one bin to another
    if (frame > start_) {
      if (i1 != previous_i1 || i2 != previous_i2) {
        ++transitions[previous_i1][previous_i2];
        // DEBUG - Report transition
        if ( debug_ > 2 ) {
          if ( (i1 == previous_i1 || i2 == previous_i2) &&
               ( (i1 != previous_i1 && (abs(i1 - previous_i1) == 1)) ||
                 (i2 != previous_i2 && (abs(i2 - previous_i2) == 1)) ||
                 (i1 != previous_i1 && (abs(i1 - previous_i1) == 5)) ||
                 (i2 != previous_i2 && (abs(i2 - previous_i2) == 5)) ) ) 
          {
            // SMALL transition
            if (type_==DISTANCE) {
              mprintf("SMALL TRANSITION frame %6i (%s,%s): substate (%s) to (%s)\n",
                      frame, scalar1_->c_str(), scalar2_->c_str(),
                      distance_ss_2D[previous_i1][previous_i2], distance_ss_2D[i1][i2]);
            } else {
              mprintf("SMALL TRANSITION frame %6i (%s,%s): substate (%s) to (%s)\n",
                      frame, scalar1_->c_str(), scalar2_->c_str(),
                      torsion_ss_2D[previous_i1][previous_i2], torsion_ss_2D[i1][i2]);
            }
          } else {
            // LARGE transition
            if (type_==DISTANCE) {
              mprintf("LARGE TRANSITION frame %6i (%s,%s): substate (%s) to (%s)\n",
                      frame, scalar1_->c_str(), scalar2_->c_str(),
                      distance_ss_2D[previous_i1][previous_i2], distance_ss_2D[i1][i2]);
            } else {
              mprintf("LARGE TRANSITION frame %6i (%s,%s): substate (%s) to (%s)\n",
                      frame, scalar1_->c_str(), scalar2_->c_str(),
                      torsion_ss_2D[previous_i1][previous_i2], torsion_ss_2D[i1][i2]);
            }
          }
        }
      }
    } // END transition check 
    previous_i1 = i1;
    previous_i2 = i2;
  } // END loop over frames

  // after processing all frames, compute the averages and standard deviations
  for (int j=0;j<6;j++) {
    for (int k=0;k<6;k++) {

      if (visits[j][k]) {
        v1_avg[j][k] = v1_avg[j][k]/visits[j][k];
        v2_avg[j][k] = v2_avg[j][k]/visits[j][k];
      }
      if (visits[j][k] > 1) {
        v1_sd[j][k] = v1_sd[j][k]/visits[j][k];
        v2_sd[j][k] = v2_sd[j][k]/visits[j][k];
        v1_sd[j][k] = sqrt( v1_sd[j][k] - v1_avg[j][k]*v1_avg[j][k] );
        v2_sd[j][k] = sqrt( v2_sd[j][k] - v2_avg[j][k]*v2_avg[j][k] );
      } else {
        v1_sd[j][k] = 0.0;
        v2_sd[j][k] = 0.0;
      }
      if (visits[j][k]) {
        v1_avg[j][k] -= torsion_offset[j];
        v2_avg[j][k] -= torsion_offset[k];
      }
    }
  }

  // NOTE: In original ptraj code output was closed here. Keep using
  // output file for easy redirection of results.
  
  // PRINT RESULTS
  const char* initial_label = 0;
  const char* final_label = 0;
  if (type_ == ANGLE) {
    initial_label = torsion_ss_2D[initial_i1][initial_i2];
    final_label = torsion_ss_2D[final_i1][final_i2];
  } else { // if type_==DISTANCE
    initial_label = distance_ss_2D[initial_i1][initial_i2];
    final_label =  distance_ss_2D[final_i1][final_i2];
  }
  outfile.Printf("\n\nCRANKSHAFT: %s.\n", info_.c_str());
  outfile.Printf("  start at frame %i, stop after frame %i, offset between frames is %i.\n",
                 start_+1, stop_, offset_);
  outfile.Printf("  total number of frames is %i.  Table values are\n", totalFrames);
  outfile.Printf("  %%occupied, #transitions to another substate, average angles and stddev\n");
  outfile.Printf("\n  INITIAL VALUE: %s (%6.1f, %6.1f)\n", initial_label, initial_v1, initial_v2);
  outfile.Printf("  FINAL VALUE:   %s (%6.1f, %6.1f)\n\n", final_label, final_v1, final_v2);

  // Supplementary information based on type of crankshaft
  if (angletype_ == EPSILON_ZETA) {
    // epsilon/zeta in nucleic acids!
    i1 = 0;
    i2 = 0;
    for (int i=start_; i < stop_; i+=offset_) {
      double v1 = scalar1_->Dval(i);
      double v2 = scalar2_->Dval(i);
      if (v1 < 0) v1 += 360.0;
      if (v2 < 0) v2 += 360.0;
      v1 = v1 - v2;
      if (v1 > 60 && v1 < 120) i2++;
      if (v1 < -30 && v1 > -120) i1++;
    }
    outfile.Printf("    EPSILON/ZETA crankshaft\n");
    outfile.Printf("      BI  = (t, g-) or eps-zeta ~ -90 [currently = %.1f%%]\n",
                   i1*100.0 / totalFrames);
    outfile.Printf("      BII = (g-, t) or eps-zeta ~ +90 [currently = %.1f%%]\n\n",
                   i2*100.0 / totalFrames);

  } else if (angletype_ == ALPHA_GAMMA) {
    // alpha/gamma in nucleic acids!
    outfile.Printf("    ALPHA/GAMMA crankshaft\n");
    outfile.Printf("      canonical is (g-, g+) [currently at %.1f%%]\n",
                   visits[4][0]*100.0/totalFrames);
    outfile.Printf("      other possible states are (t, t) {%.1f%%} or (t, g-) {%.1f%%}\n",
                   visits[2][2]*100.0/totalFrames,
                   visits[2][4]*100.0/totalFrames);
    outfile.Printf("      (g+, t) is found < 5%% in protein/DNA complexes {%.1f%%}\n",
                   visits[0][2]*100.0/totalFrames);
    if ( (visits[0][2] + visits[1][2])*100.0/totalFrames > 10.0 )
      outfile.Printf("    *** > 10%% population in (g+, t) / (a+, t) states!!!\n");
    outfile.Printf("\n");
  }

  outfile.Printf("                %s         %s         %s         %s         %s          %s\n",
          torsion_ss[0],torsion_ss[1],torsion_ss[2],
          torsion_ss[3],torsion_ss[4],torsion_ss[5]);
  outfile.Printf("        -------------------------------------------------------------------------------------------------\n");
  for (int i=0; i < 6; i++) {
    outfile.Printf("        |");
    for (int j=0; j < 6; j++) {
      if (visits[i][j] == 0)
        outfile.Printf("               |");
      else
        outfile.Printf("   %7.1f%%    |", 100.0 * visits[i][j]/totalFrames);
    }
    outfile.Printf("\n");

    outfile.Printf(" %s|", torsion_ss[i]);
    for (int j=0; j < 6; j++) {
      if (visits[i][j] == 0)
        outfile.Printf("               |");
      else
        outfile.Printf(" %8i      |", transitions[i][j]);
    }
    outfile.Printf("\n");

    outfile.Printf("        |");
    for (int j=0; j < 6; j++) {
      if (visits[i][j] == 0)
        outfile.Printf("               |");
      else
        outfile.Printf(" %6.1f %6.1f |", v1_avg[i][j], v2_avg[i][j]);
    }
    outfile.Printf("\n");

    outfile.Printf("        |");
    for (int j=0; j < 6; j++) {
      if (visits[i][j] < 2)
        outfile.Printf("               |");
      else
        outfile.Printf(" %6.1f %6.1f |", v1_sd[i][j], v2_sd[i][j]);
    }
    outfile.Printf("\n");

    outfile.Printf("        |-----------------------------------------------------------------------------------------------|\n");
  }

  return 0;
}

