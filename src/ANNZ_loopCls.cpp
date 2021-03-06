// ===========================================================================================================
// Copyright (C) 2015, Iftach Sadeh
// 
// This file is part of ANNZ.
// ANNZ is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ===========================================================================================================

// ===========================================================================================================
/**
 * @brief  - Find the "best" clasification MLM, based on the separation parameter (see getSeparation())
 *         and produce some performance plots.
 */
// ===========================================================================================================
void ANNZ::optimCls() {
// ===========================================================================================================
  aLOG(Log::INFO) <<coutWhiteOnBlack<<coutYellow<<" - starting ANNZ::optimCls() ... "<<coutDef<<endl;

  TString MLMname(""), hisName(""), sysCmnd(""), clasProbHisDirName("");
  int     nSbFracs(0), nSbFracPlots(5), nCompPureMgs(1);
  
  int     nMLMs             = glob->GetOptI("nMLMs");
  TString sigBckTypeName    = glob->GetOptC("sigBckTypeName");
  // bool    separateTestValid = glob->GetOptB("separateTestValid"); // deprecated
  int     nANNZtypes        = (int)allANNZtypes.size();

  // adjust maxNobj to stop loop after maxNobj/2 sig and maxNobj/2 bck objects have been accepted
  int     maxNobj          = glob->GetOptI("maxNobj");
  if(maxNobj == 0) maxNobj = -1; 
  else             maxNobj = static_cast<int>(floor(0.1 + maxNobj/2.));

  double  normFactor(0);
  int     hisBinsN(0), compPureN(0), rebinFactor(0), bufSize(glob->GetOptI("hisBufSize"));

  // create the chain for the loop
  // -----------------------------------------------------------------------------------------------------------
  // TString treeNamePostfix = (TString)(separateTestValid ? "_valid" : "_train"); // deprecated
  TString treeNamePostfix = (TString)"_train";
  TString inTreeName      = (TString)glob->GetOptC("treeName")+treeNamePostfix;
  TString inFileName      = (TString)glob->GetOptC("postTrainDirNameFull")+inTreeName+"*.root";

  // prepare the chain and input variables. Set cuts to match the TMVAs
  // -----------------------------------------------------------------------------------------------------------
  TChain * aChain = new TChain(inTreeName,inTreeName); aChain->SetDirectory(0); aChain->Add(inFileName); 
  int nEntriesChain = aChain->GetEntries();
  aLOG(Log::DEBUG) <<coutRed<<" - added chain "<<coutGreen<<inTreeName<<"("<<nEntriesChain<<")"<<" from "<<coutBlue<<inFileName<<coutDef<<endl;

  // connect the variables to the tree (and add formulae for all the input parameters of the readers)
  VarMaps * var = new VarMaps(glob,utils,"loopRegClsVar");
  var->connectTreeBranches(aChain);

  // if(separateTestValid) var->setTreeCuts("_train",getTrainTestCuts("_valid",0,0,0,var)); // deprecated

  // number of initial bins and rebin factor for classification response histograms
  hisBinsN  = glob->GetOptI("clsResponseHisN");  rebinFactor = glob->GetOptI("clsResponseHisR");
  compPureN = static_cast<int>(floor(hisBinsN/double(rebinFactor)));

  vector <TH1*>                                 hisSbSepV;
  vector < vector <TH1*> >                      hisSbSepVV;
  map < TString,int >                           maxSbSepFracIndexM;
  vector < int >                                sbFracPlotV;
  map < TString , vector < pair<int,double> > > sbSepFracIndexM;

  // setup histograms and multiGraphs for the signal, background and separation
  // -----------------------------------------------------------------------------------------------------------
  map < TString , vector <TMultiGraph*> > compPureMgV;
  compPureMgV["CLS"].resize(nCompPureMgs); compPureMgV["PRB"].resize(nCompPureMgs); compPureMgV["ERR"].resize(nCompPureMgs);
  for(int nCompPureMgNow=0; nCompPureMgNow<nCompPureMgs; nCompPureMgNow++) {
    compPureMgV["CLS"][nCompPureMgNow] = new TMultiGraph();
    compPureMgV["PRB"][nCompPureMgNow] = new TMultiGraph();
    compPureMgV["ERR"][nCompPureMgNow] = new TMultiGraph();
  }

  map < TString , map <int,TH1*> >  his1M;
  for(int nMLMnow=0; nMLMnow<nMLMs; nMLMnow++) {
    MLMname = getTagName(nMLMnow); if(mlmSkip[MLMname]) continue;

    int    numBins0;
    double hisRange0, hisRange1; 

    // MLM response histograms
    hisRange0 = 1; hisRange1 = -1; numBins0 = 10000;
    hisName  = (TString)MLMname+"_clsOptim"+"_SIG";                his1M["SIG"][nMLMnow] = new TH1F(hisName,hisName,numBins0,hisRange0,hisRange1);
    his1M["SIG"][nMLMnow]->SetDirectory(0);                        his1M["SIG"][nMLMnow]->SetDefaultBufferSize(bufSize);
    
    hisName  = (TString)MLMname+"_clsOptim"+"_BCK";                his1M["BCK"][nMLMnow] = (TH1*)his1M["SIG"][nMLMnow]->Clone(hisName);
    his1M["BCK"][nMLMnow]->SetDirectory(0);                        his1M["BCK"][nMLMnow]->SetDefaultBufferSize(bufSize);

    // MLM probability histograms
    hisRange0 = 0; hisRange1 = 1; numBins0 = compPureN;
    hisName  = (TString)MLMname+"_clsOptim"+"_prbSig";             his1M["prbSig"][nMLMnow] = new TH1F(hisName,hisName,numBins0,hisRange0,hisRange1);
    his1M["prbSig"][nMLMnow]->SetTitle(hisName);                   his1M["prbSig"][nMLMnow]->SetDirectory(0);
    his1M["prbSig"][nMLMnow]->GetXaxis()->SetTitle((TString)"p");  his1M["prbSig"][nMLMnow]->GetYaxis()->SetTitle("1/n #times dn/dp");
    
    hisName  = (TString)MLMname+"_clsOptim"+"_prbBck";             his1M["prbBck"][nMLMnow] = (TH1*)his1M["prbSig"][nMLMnow]->Clone(hisName);
    his1M["prbBck"][nMLMnow]->SetTitle(hisName);                   his1M["prbBck"][nMLMnow]->SetDirectory(0);
    his1M["prbBck"][nMLMnow]->GetXaxis()->SetTitle((TString)"p");  his1M["prbBck"][nMLMnow]->GetYaxis()->SetTitle("1/n #times dn/dp");
  }

  map < TString , map <TMVA::Types::EMVA,TH1*> >  hisSepDistM;
  for(int nANNZtypeNow=0; nANNZtypeNow<nANNZtypes; nANNZtypeNow++) {
    double hisRange0(0), hisRange1(1); int numBins0(100);

    hisName  = TString::Format((TString)"nANNZtype_%d"+"_clsOptim"+"CLS"+"_sepDist",nANNZtypeNow);

    TH1 * his1 = new TH1F(hisName,hisName,numBins0,hisRange0,hisRange1);
    his1->SetTitle(typeToNameMLM[allANNZtypes[nANNZtypeNow]]); his1->SetDirectory(0); his1->SetDefaultBufferSize(bufSize);
    his1->GetXaxis()->SetTitle((TString)"S_{s/b}"); his1->GetYaxis()->SetTitle("1/S_{s/b} #times dS_{s/b}/dn_{MLM}");
    
    hisSepDistM["CLS"][allANNZtypes[nANNZtypeNow]] = his1;
    
    hisName.ReplaceAll("CLS","PRB");
    hisSepDistM["PRB"][allANNZtypes[nANNZtypeNow]] = (TH1*)his1->Clone(hisName);
  }

  // loop on the tree
  // -----------------------------------------------------------------------------------------------------------
  bool  breakLoop(false), mayWriteObjects(false);
  int   nObjectsToWrite(glob->GetOptI("nObjectsToWrite"));
  var->clearCntr();
  for(Long64_t loopEntry=0; true; loopEntry++) {
    if(!var->getTreeEntry(loopEntry)) breakLoop = true;

    if((mayWriteObjects && var->GetCntr("nObj") % nObjectsToWrite == 0) || breakLoop) {
      var->printCntr(inTreeName); mayWriteObjects = false;
    }
    if(breakLoop) break;

    // if(separateTestValid) { if(var->hasFailedTreeCuts("_train")) continue; } // deprecated

    int sigBckType = var->GetVarI(sigBckTypeName);
    if(sigBckType == -1) {
      var->IncCntr("fail sig/bck cuts"); continue;
    }
    var->IncCntr("nObj"); // if passed cuts, increment the object counter
    
    bool isBck = (sigBckType == 0);

    if(maxNobj > 0) {
      if(isBck) { var->IncCntr("nObj_bck all looped"); if(var->GetCntr("nObj_bck") == maxNobj) continue; }
      else      { var->IncCntr("nObj_sig all looped"); if(var->GetCntr("nObj_sig") == maxNobj) continue; }
    }

    for(int nMLMnow=0; nMLMnow<nMLMs; nMLMnow++) {
      MLMname  = getTagName(nMLMnow); if(mlmSkip[MLMname]) continue;

    //TString MLMname_e = getTagError(nMLMnow);
      TString MLMname_w = getTagWeight(nMLMnow);
      TString MLMname_v = getTagClsVal(nMLMnow);

      double  clsPrb    = var->GetVarF(MLMname);    clsPrb = min(max(clsPrb,0.001),0.999); //avoid histogram overflow bins
      double  clasVal   = var->GetVarF(MLMname_v);
      double  weightNow = var->GetVarF(MLMname_w);  if(weightNow < EPS) continue;

      if(isBck) { his1M["BCK"][nMLMnow]->Fill(clasVal,weightNow); his1M["prbBck"][nMLMnow]->Fill(clsPrb,weightNow); }
      else      { his1M["SIG"][nMLMnow]->Fill(clasVal,weightNow); his1M["prbSig"][nMLMnow]->Fill(clsPrb,weightNow); }
    }

    // to increment the loop-counter, at least one method should have passed the cuts
    mayWriteObjects = true; if(isBck) var->IncCntr("nObj_bck"); else var->IncCntr("nObj_sig");
    if(var->GetCntr("nObj_sig") == maxNobj && var->GetCntr("nObj_bck") == maxNobj) breakLoop = true;
  }
  if(!breakLoop) var->printCntr(inTreeName);


  // -----------------------------------------------------------------------------------------------------------
  // create the sig/bck histograms with the final binning
  // -----------------------------------------------------------------------------------------------------------
  for(int nMLMnow=0; nMLMnow<nMLMs; nMLMnow++) {
    MLMname = getTagName(nMLMnow); if(mlmSkip[MLMname]) continue;

    // derive proper histogram limits
    his1M["SIG"][nMLMnow]->BufferEmpty(); his1M["BCK"][nMLMnow]->BufferEmpty();

    int     hisBins(0), hasQuant(0);
    double  hisRange0(0), hisRange1(0);
    if(his1M["SIG"][nMLMnow]->Integral() > 0 && his1M["BCK"][nMLMnow]->Integral() > 0) {
      vector <double> fracV, quantV_SIG, quantV_BCK;
      double minFracForLimit(1e-4); fracV.push_back(minFracForLimit); fracV.push_back(1-minFracForLimit);

      utils->param->clearAll(); hasQuant = utils->getQuantileV(fracV,quantV_SIG,his1M["SIG"][nMLMnow]);
      VERIFY(LOCATION,(TString)"Could not do utils->getQuantileV("+his1M["SIG"][nMLMnow]->GetName()+") !?!",hasQuant);
      
      utils->param->clearAll(); hasQuant = utils->getQuantileV(fracV,quantV_BCK,his1M["BCK"][nMLMnow]);
      VERIFY(LOCATION,(TString)"Could not do utils->getQuantileV("+his1M["BCK"][nMLMnow]->GetName()+") !?!",hasQuant);

      hisBins   = compPureN;
      hisRange0 = min(quantV_SIG[0],quantV_BCK[0]);  hisRange1 = max(quantV_SIG[1],quantV_BCK[1]);

      utils->param->clearAll(); fracV.clear(); quantV_SIG.clear(); quantV_BCK.clear();
    }

    // define and fill the nice-looking histograms
    hisName  = (TString)MLMname+"_clsOptim"+"_sig";                his1M["sig"][nMLMnow] = new TH1F(hisName,hisName,hisBins,hisRange0,hisRange1);
    his1M["sig"][nMLMnow]->SetTitle(hisName);                      his1M["sig"][nMLMnow]->SetDirectory(0);
    his1M["sig"][nMLMnow]->GetXaxis()->SetTitle((TString)"#eta");  his1M["sig"][nMLMnow]->GetYaxis()->SetTitle("1/n #times dn/d#eta");

    hisName  = (TString)MLMname+"_clsOptim"+"_bck";                his1M["bck"][nMLMnow] = (TH1*)his1M["sig"][nMLMnow]->Clone(hisName);
    his1M["bck"][nMLMnow]->SetTitle(hisName);                      his1M["bck"][nMLMnow]->SetDirectory(0);
    his1M["bck"][nMLMnow]->GetXaxis()->SetTitle((TString)"#eta");  his1M["bck"][nMLMnow]->GetYaxis()->SetTitle("1/n #times dn/d#eta");

    for(int nSigBckNow=0; nSigBckNow<2; nSigBckNow++) {
      TString sigBckName0 = (TString)((nSigBckNow == 0) ? "SIG" : "BCK");
      TString sigBckName1 = (TString)((nSigBckNow == 0) ? "sig" : "bck");

      int nBinL = his1M[sigBckName0][nMLMnow]->GetXaxis()->FindBin(hisRange0);
      int nBinH = his1M[sigBckName0][nMLMnow]->GetXaxis()->FindBin(hisRange1);
      for(int nBinNow=nBinL; nBinNow<nBinH+1; nBinNow++) {
        double  binCenter  = his1M[sigBckName0][nMLMnow]->GetBinCenter (nBinNow);
        double  binContent = his1M[sigBckName0][nMLMnow]->GetBinContent(nBinNow);

        if(binContent > EPS) his1M[sigBckName1][nMLMnow]->Fill(binCenter,binContent);
      }
    }
  }


  // -----------------------------------------------------------------------------------------------------------
  // rank the MLM performance by the sig/bck separation
  // -----------------------------------------------------------------------------------------------------------
  for(int nTypeNow=0; nTypeNow<2; nTypeNow++) {
    TString typeName(""), typeNameSig(""), typeNameBck(""), typeTitleSig(""), typeTitleBck("");
    if     (nTypeNow == 0) { typeName = "CLS"; typeNameSig = "sig";    typeNameBck= "bck";    typeTitleSig = "Sig";     typeTitleBck = "Bck";     }
    else if(nTypeNow == 1) { typeName = "PRB"; typeNameSig = "prbSig"; typeNameBck= "prbBck"; typeTitleSig = "P_{sig}"; typeTitleBck = "P_{bck}"; }

    // -----------------------------------------------------------------------------------------------------------
    // calculate the separation and normalize the sig/bck histograms
    // -----------------------------------------------------------------------------------------------------------
    sbSepFracIndexM[typeName].clear(); sbSepFracIndexM[typeName].reserve(nMLMs);

    for(int nMLMnow=0; nMLMnow<nMLMs; nMLMnow++) {
      MLMname = getTagName(nMLMnow); if(mlmSkip[MLMname]) continue;

      normFactor = his1M[typeNameSig][nMLMnow]->Integral(); if(normFactor>0) his1M[typeNameSig][nMLMnow]->Scale(1/normFactor);
      normFactor = his1M[typeNameBck][nMLMnow]->Integral(); if(normFactor>0) his1M[typeNameBck][nMLMnow]->Scale(1/normFactor);

      // see "separation" in: http://root.cern.ch/root/html/ANNZ__MethodBase.html
      double sbSepFrac = getSeparation(his1M[typeNameSig][nMLMnow],his1M[typeNameBck][nMLMnow]);
      if(sbSepFrac < EPS) sbSepFrac = EPS; else if(sbSepFrac > (1-EPS)) sbSepFrac = 1-EPS;

      sbSepFracIndexM[typeName].push_back(pair<int,double>(nMLMnow,sbSepFrac));
      hisSepDistM[typeName][typeMLM[nMLMnow]]->Fill(sbSepFrac);
    }
    nSbFracs = (int)sbSepFracIndexM[typeName].size();

    // no point in going on if there is less than two MLMs to compare
    if(nSbFracs < 2) {
      aLOG(Log::INFO) <<coutPurple<<" - found only "<<coutRed<<nSbFracs<<coutPurple<<" MLM to compare. No need to create ranking ... "<<coutDef<<endl;
      continue;
    }

    // sort the methods by the separation fraction
    // -----------------------------------------------------------------------------------------------------------
    sort(sbSepFracIndexM[typeName].begin(),sbSepFracIndexM[typeName].end(),sortFunc::pairID::highToLowBy1);

    maxSbSepFracIndexM[typeName] = sbSepFracIndexM[typeName][0].first;

    // select nSbFracPlots indices (as evenly-spaced as possible) spanning the range of indices of the nSbFracs methods
    if(nSbFracs < nSbFracPlots) nSbFracPlots = nSbFracs;

    sbFracPlotV.resize(nSbFracPlots);
    for(int nIndexNow=0; nIndexNow<nSbFracPlots; nIndexNow++) {
      sbFracPlotV[nIndexNow] = static_cast<int>(floor(nIndexNow * nSbFracs / double(nSbFracPlots)));
    }
    sbFracPlotV[nSbFracPlots-1] = nSbFracs-1;

    // completeness vs. purity graph
    // -----------------------------------------------------------------------------------------------------------
    for(int nPlotSbSepNow=0; nPlotSbSepNow<nSbFracPlots; nPlotSbSepNow++) {
      int     nSbSepIndexNow = sbFracPlotV[nPlotSbSepNow];
      int     nMLMnow        = sbSepFracIndexM[typeName][nSbSepIndexNow].first;
      double  sbSepFrac      = sbSepFracIndexM[typeName][nSbSepIndexNow].second;

      TH1             * hisSig(his1M[typeNameSig][nMLMnow]), * hisBck(his1M[typeNameBck][nMLMnow]);
      vector <double> graph_X, graph_Y, graph_Xerr, graph_Yerr;

      int nBins = hisSig->GetNbinsX();
      for(int nCompPureMgNow=0; nCompPureMgNow<nCompPureMgs; nCompPureMgNow++) {
        graph_X.clear(); graph_Y.clear(); graph_Xerr.clear(); graph_Yerr.clear();

        for(int nCompPureNow=1; nCompPureNow<nBins+1; nCompPureNow++) {
          double  intgrSig  = hisSig->Integral(nCompPureNow,nBins);
          double  intgrBck  = hisBck->Integral(nCompPureNow,nBins);  if(intgrSig+intgrBck < EPS) continue;
          double  comp      = intgrSig;
          double  pure      = intgrSig/(intgrSig+intgrBck);

          graph_X   .push_back(comp); graph_Y   .push_back(pure);
          graph_Xerr.push_back(EPS);  graph_Yerr.push_back(EPS);
        }
        if((int)graph_X.size() == 0) continue;

        TGraphErrors * grph = new TGraphErrors(int(graph_X.size()),&graph_X[0], &graph_Y[0],&graph_Xerr[0], &graph_Yerr[0]);
        
        grph->SetName(TString::Format((TString)"compPure_%d"+"_clsOptim"+typeName+"_%d",nCompPureMgNow,nPlotSbSepNow));
        grph->SetTitle(TString::Format((TString)"ranked as #%d, S_{s/b} ("+getTagName(nMLMnow)+","+typeToNameMLM[typeMLM[nMLMnow]]+") = %1.2e",nSbSepIndexNow+1,sbSepFrac));
        grph->GetXaxis()->SetTitle("Completeness");  grph->GetYaxis()->SetTitle("Purity");
        compPureMgV[typeName][nCompPureMgNow]->Add(grph);
      }
      graph_X.clear(); graph_Y.clear(); graph_Xerr.clear(); graph_Yerr.clear();
    }

    // rebin and normalize all sig/bck histograms to differential distributions
    // -----------------------------------------------------------------------------------------------------------
    for(int nMLMnow=0; nMLMnow<nMLMs; nMLMnow++) {
      MLMname = getTagName(nMLMnow); if(mlmSkip[MLMname]) continue;

      // find the element in the sorted sbSepFracIndexM[typeName], which corresponds to this nMLMnow
      vector < pair<int,double> >::iterator Itr;
      Itr = find_if(sbSepFracIndexM[typeName].begin(), sbSepFracIndexM[typeName].end(), sortFunc::pairID::equalToFirst(nMLMnow));

      int nSbSepIndexNow(-1); double nSbSepValNow(-1);
      if(Itr != sbSepFracIndexM[typeName].end()) { // just a sanity check, really
        nSbSepIndexNow = (int)distance(sbSepFracIndexM[typeName].begin(),Itr);
        nSbSepValNow   = Itr->second;
      }

      for(int nSigBckNow=0; nSigBckNow<2; nSigBckNow++) {
        TString sigBckName(""), sigBckTitle("");
        if     (nSigBckNow == 0) { sigBckName = typeNameSig; sigBckTitle = typeTitleSig; }
        else if(nSigBckNow == 1) { sigBckName = typeNameBck; sigBckTitle = typeTitleBck; }

        normFactor = his1M[sigBckName][nMLMnow]->Integral(); if(normFactor>0) his1M[sigBckName][nMLMnow]->Scale(1/normFactor,"width");

        his1M[sigBckName][nMLMnow]->SetTitle( TString::Format( (TString)"ranked as #%d, "+sigBckTitle+" ("+MLMname+","
                                                                        +typeToNameMLM[typeMLM[nMLMnow]]+") - S_{s/b} = %1.2e"
                                                               ,nSbSepIndexNow+1,nSbSepValNow ) );
      }
    }

    // -----------------------------------------------------------------------------------------------------------
    // plot with the dist of separation fraction and the best/worst methods
    // ----------------------------------------------------------------------------------------------------------- 
    // normalize the separation fraction to a differential distribution and add it to the vector for plotting
    hisSbSepVV.clear();  hisSbSepV.clear();
    for(int nANNZtypeNow=0; nANNZtypeNow<nANNZtypes; nANNZtypeNow++) { 
      normFactor = hisSepDistM[typeName][allANNZtypes[nANNZtypeNow]]->Integral();
      if(normFactor>0) hisSepDistM[typeName][allANNZtypes[nANNZtypeNow]]->Scale(1/normFactor,"width");

      hisSbSepV.push_back(hisSepDistM[typeName][allANNZtypes[nANNZtypeNow]]);
    }
    hisSbSepVV.push_back(hisSbSepV);

    // add the selected sig/bck histograms for the vector for plotting
    for(int nPlotSbSepNow=0; nPlotSbSepNow<nSbFracPlots; nPlotSbSepNow++) {
      int     nSbSepIndexNow = sbFracPlotV[nPlotSbSepNow];
      int     nMLMnow        = sbSepFracIndexM[typeName][nSbSepIndexNow].first;

      hisSbSepV.clear();
      hisSbSepV.push_back(his1M[typeNameSig][nMLMnow]); hisSbSepV.push_back(his1M[typeNameBck][nMLMnow]);
      hisSbSepVV.push_back(hisSbSepV);
    }

    outputs->optClear();
    outputs->draw->NewOptB("doIndividualPlots",glob->OptOrNullB("doIndividualPlots"));
    outputs->draw->NewOptI("nPadsRow" , static_cast<int>(floor((nSbFracPlots+1)/2.)));
    outputs->draw->NewOptB("multiCnvs" , true);
    outputs->draw->NewOptC("drawOpt" , "HIST");
    outputs->draw->NewOptC("maxDrawMark" , "100");   
    outputs->drawHis1dMultiV(hisSbSepVV);
  }

  // compare the max separation from both types    
  // -----------------------------------------------------------------------------------------------------------
  hisSbSepVV.clear();
  for(int nTypeNow0=0; nTypeNow0<2; nTypeNow0++) {
    if(nTypeNow0 == 1) { if(maxSbSepFracIndexM["CLS"] == maxSbSepFracIndexM["PRB"]) continue; }

    for(int nTypeNow1=0; nTypeNow1<2; nTypeNow1++) {
      TString typeName(""), typeNameSig(""), typeNameBck(""), typeTitleBck("");
      if     (nTypeNow1 == 0) { typeName = "CLS"; typeNameSig = "sig";    typeNameBck= "bck";    }
      else if(nTypeNow1 == 1) { typeName = "PRB"; typeNameSig = "prbSig"; typeNameBck= "prbBck"; }

      int nMLMnow = (nTypeNow0 == 0) ? maxSbSepFracIndexM["CLS"] : maxSbSepFracIndexM["PRB"];

      hisSbSepV.clear();
      hisSbSepV.push_back(his1M[typeNameSig][nMLMnow]); hisSbSepV.push_back(his1M[typeNameBck][nMLMnow]);
      hisSbSepVV.push_back(hisSbSepV);
    }
  }

  outputs->optClear();
  outputs->draw->NewOptB("doIndividualPlots",glob->OptOrNullB("doIndividualPlots"));
  if(maxSbSepFracIndexM["CLS"] == maxSbSepFracIndexM["PRB"]) {
    outputs->draw->NewOptC("generalHeader_0" , "Classifier response (max S_{s/b})");
    outputs->draw->NewOptC("generalHeader_1" , "Classifier probability (max S_{s/b})");
  }
  else {
    outputs->draw->NewOptC("generalHeader_0" , "Classifier response (max S_{s/b})");
    outputs->draw->NewOptC("generalHeader_1" , "Classifier probability");
    outputs->draw->NewOptC("generalHeader_2" , "Classifier response");
    outputs->draw->NewOptC("generalHeader_3" , "Classifier probability (max S_{s/b})");
  }
  outputs->draw->NewOptI("nPadsRow" , 2);
  outputs->draw->NewOptB("multiCnvs" , true);
  outputs->draw->NewOptC("drawOpt" , "HIST");
  outputs->draw->NewOptC("maxDrawMark" , "100");   
  outputs->drawHis1dMultiV(hisSbSepVV);

  // draw the purity/completeness multigraphs
  // -----------------------------------------------------------------------------------------------------------
  for(int nCompPureMgNow=0; nCompPureMgNow<nCompPureMgs; nCompPureMgNow++) {
    vector <TMultiGraph*> mGraphV;
    mGraphV.push_back(compPureMgV["CLS"][nCompPureMgNow]);
    mGraphV.push_back(compPureMgV["PRB"][nCompPureMgNow]);

    outputs->optClear();
    outputs->draw->NewOptB("doIndividualPlots",glob->OptOrNullB("doIndividualPlots"));
    outputs->draw->NewOptC("generalHeader_0" , "Response");
    outputs->draw->NewOptC("generalHeader_1" , "Probability");
    outputs->draw->NewOptB("multiCnvs" , true);
    outputs->draw->NewOptC("drawOpt" , "alp");
    outputs->draw->NewOptB("setGridX" , true);
    outputs->draw->NewOptB("setGridY" , true);
    outputs->drawMultiGraphV(mGraphV);

    mGraphV.clear();
  }

  // print out the plots
  outputs->WriteOutObjects(true,true); outputs->ResetObjects();


  // -----------------------------------------------------------------------------------------------------------
  // store the results of the optimization to file
  // -----------------------------------------------------------------------------------------------------------
  map <TString,TString> orderedMLMs;
  for(int nTypeNow=0; nTypeNow<2; nTypeNow++) {
    TString typeName(""), typeNameSig(""), typeNameBck(""), typeTitleSig(""), typeTitleBck("");
    if     (nTypeNow == 0) { typeName = "PRB"; }
    else if(nTypeNow == 1) { typeName = "CLS"; }

    orderedMLMs[typeName] = "";
    int nAcceptedMLMs = (int)sbSepFracIndexM[typeName].size();
    for(int nMLMnow0=0; nMLMnow0<nAcceptedMLMs; nMLMnow0++) {
      int nMLMnow = sbSepFracIndexM[typeName][nMLMnow0].first;

      orderedMLMs[typeName] += getTagName(nMLMnow);
      if(nMLMnow0 < nAcceptedMLMs-1) orderedMLMs[typeName] += ";";
    }
    aLOG(Log::INFO)<<coutGreen<<" - Optimization by "<<coutYellow<<typeName<<coutGreen<<" - "<<coutBlue<<orderedMLMs[typeName]<<coutDef<<endl;
  }


  // save the optimization results to file
  // -----------------------------------------------------------------------------------------------------------
  TString saveFileName = getKeyWord("","optimResults","configSaveFileName");
  aLOG(Log::INFO)<<coutYellow<<" - Saving optimization results in "<<coutGreen<<saveFileName<<coutYellow<<" ..."<<coutDef<<endl;

  OptMaps * optMap = new OptMaps("localOptMap");
  TString          saveName("");
  vector <TString> optNames;

  saveName = glob->versionTag();  optNames.push_back(saveName); optMap->NewOptC(saveName, glob->GetOptC(glob->versionTag()));
  saveName = "optimMLMs_PRB";     optNames.push_back(saveName); optMap->NewOptC(saveName, orderedMLMs["PRB"]);
  saveName = "optimMLMs_CLS";     optNames.push_back(saveName); optMap->NewOptC(saveName, orderedMLMs["CLS"]);

  utils->optToFromFile(&optNames,optMap,saveFileName,"WRITE");

  optNames.clear(); DELNULL(optMap);

  DELNULL(var); DELNULL(aChain);

  return;
}



