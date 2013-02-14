#include "Action_MultiDihedral.h"
#include "DataSet_double.h"
#include "CpptrajStdio.h"
#include "Constants.h"
#include "TorsionRoutines.h"

Action_MultiDihedral::Action_MultiDihedral() :
  range360_(false),
  outfile_(0)
{}

void Action_MultiDihedral::Help() {
  mprintf("\t[<name>] [phi] [psi] [resrange <range>] [out <filename>]\n");
  //mprintf("\t[range360]\n");
}

Action::RetType Action_MultiDihedral::Init(ArgList& actionArgs, TopologyList* PFL, FrameList* FL,
                          DataSetList* DSL, DataFileList* DFL, int debugIn)
{
  // Get keywords
  outfile_ = DFL->AddDataFile( actionArgs.GetStringKey("out"), actionArgs);
  range360_ = actionArgs.hasKey("range360");
  std::string resrange_arg = actionArgs.GetStringKey("resrange");
  if (!resrange_arg.empty())
    if (resRange_.SetRange( resrange_arg )) return Action::ERR;
  if (actionArgs.hasKey("phi"))
    dihSearch_.SearchFor(DihedralSearch::PHI);
  if (actionArgs.hasKey("psi"))
    dihSearch_.SearchFor(DihedralSearch::PSI);
  // If no dihedral types yet selected, this will select all.
  dihSearch_.SearchForAll();

  // Setup DataSet(s) name
  dsetname_ = actionArgs.GetStringNext();

  mprintf("    MULTIDIHEDRAL: Calculating");
  dihSearch_.PrintTypes();
  if (!resRange_.Empty())
    mprintf(" dihedrals for residues in range %s\n", resRange_.RangeArg());
  else
    mprintf(" dihedrals for all residues.\n");
  if (!dsetname_.empty())
    mprintf("\tDataSet name: %s\n", dsetname_.c_str());
  if (outfile_ != 0) mprintf("\tOutput to %s\n", outfile_->Filename());
  if (range360_) 
    mprintf("\tRange 0-360 deg.\n");
  else
    mprintf("\tRange -180-180 deg.\n");
  masterDSL_ = DSL;
  return Action::OK;
}

// Action_MultiDihedral::Setup();
Action::RetType Action_MultiDihedral::Setup(Topology* currentParm, Topology** parmAddress) {
  Range actualRange;
  // If range is empty (i.e. no resrange arg given) look through all 
  // solute residues.
  if (resRange_.Empty())
    actualRange.SetRange(0, currentParm->FinalSoluteRes());
  else {
    // If user range specified, create new range shifted by -1 since internal
    // resnums start from 0.
    actualRange = resRange_;
    actualRange.ShiftBy(-1);
  }
  // Exit if no residues specified
  if (actualRange.Empty()) {
    mprinterr("Error: multidihedral: No residues specified for %s\n",currentParm->c_str());
    return Action::ERR;
  }
  // Search for specified dihedrals in each residue in the range
  if (dihSearch_.FindDihedrals(*currentParm, actualRange))
    return Action::ERR;

  // Print selected dihedrals, set up DataSets
  data_.clear();
  if (dsetname_.empty())
    dsetname_ = masterDSL_->GenerateDefaultName("MDIH");
  for (DihedralSearch::mask_it dih = dihSearch_.begin();
                               dih != dihSearch_.end(); ++dih)
  {
    int resNum = (*dih).ResNum() + 1;
    // See if Dataset already present
    DataSet* ds = masterDSL_->GetSet(dsetname_, resNum, (*dih).Name());
    if (ds == 0) {
      // Create new DataSet
      ds = masterDSL_->AddSetIdxAspect( DataSet::DOUBLE, dsetname_, resNum, (*dih).Name());
      // Add to outfile
      if (outfile_ != 0)
        outfile_->AddSet( ds );
    }
    if (ds != 0)
      data_.push_back( ds );
    /*mprintf("\tDIH: %s", currentParm->TruncResAtomName(*(atom++)).c_str());
    mprintf("-%s", currentParm->TruncResAtomName(*(atom++)).c_str());
    mprintf("-%s", currentParm->TruncResAtomName(*(atom++)).c_str());
    mprintf("-%s\n", currentParm->TruncResAtomName(*atom).c_str());*/
    mprintf("\tDIH [%s]:", ds->Legend().c_str());
    mprintf(" :%i@%i",   (*currentParm)[(*dih).A0()].ResNum()+1, (*dih).A0() + 1);
    mprintf(" :%i@%i",   (*currentParm)[(*dih).A1()].ResNum()+1, (*dih).A1() + 1);
    mprintf(" :%i@%i",   (*currentParm)[(*dih).A2()].ResNum()+1, (*dih).A2() + 1);
    mprintf(" :%i@%i\n", (*currentParm)[(*dih).A3()].ResNum()+1, (*dih).A3() + 1);
  }
  return Action::OK;
}

// Action_MultiDihedral::DoAction()
Action::RetType Action_MultiDihedral::DoAction(int frameNum, Frame* currentFrame, 
                                               Frame** frameAddress)
{
  std::vector<DataSet*>::iterator ds = data_.begin();
  for (DihedralSearch::mask_it dih = dihSearch_.begin();
                               dih != dihSearch_.end(); ++dih, ++ds)
  {
    double torsion = Torsion( currentFrame->XYZ((*dih).A0()),
                              currentFrame->XYZ((*dih).A1()),
                              currentFrame->XYZ((*dih).A2()),
                              currentFrame->XYZ((*dih).A3()) );
    torsion *= RADDEG;
    (*ds)->Add(frameNum, &torsion);
  }
  return Action::OK;
}

void Action_MultiDihedral::Print() {

}
