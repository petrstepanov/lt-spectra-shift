/// \file
/// \ingroup tutorial_fit
/// \notebook -js
/// Tutorial for convolution of two functions
///
/// \macro_image
/// \macro_output
/// \macro_code
///
/// \author Aurelie Flandi

#include <iostream>
#include <TString.h>
#include <TList.h>
#include <TH1.h>
#include <TFileInfo.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TSystem.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <utility>
#include <TObjString.h>

// Constants
struct Constants {
	const UInt_t headerLines = 12; // header lines number
	const Int_t epsilon = 10; // epsilon-bins around the maimum to fit
};
Constants constants;

// Spectrum - imports itself
class Spectrum : public TObject{
public:
	Spectrum(const char* fNamePath) : TObject() {
		fileNamePath = new TString(fNamePath);
		header = new TList();
		footer = new TList();
		histogram = new TH1I();
		mean = 0;
	}

	TString* fileNamePath;
	TList* header;
	TList* footer;
	TH1* histogram;
	Double_t mean;

	void init(){
		// Open file
		std::cout << fileNamePath->Data() << std::endl; // test
		std::ifstream inFile(fileNamePath->Data());
		if (!inFile) {
			std::cout << "File \"" << fileNamePath->Data() << "\" not found." << std::endl;
			exit(1);
		}

		// Read header
		for (UInt_t i = 0; i < constants.headerLines; i++) {
			std::string sLine = "";
			std::getline(inFile, sLine);
			TObjString* line = new TObjString(sLine.c_str());
			header->Add(line);
			std::cout << line->GetString().Data(); // test
		}

		// Get number of lines (last number in last header line)
		TString lastHeaderLine = ((TObjString*)header->Last())->GetString();
		TObjArray *tx = lastHeaderLine.Tokenize(" ");
		TString nEntriesString = ((TObjString*)tx->Last())->GetString();
		UInt_t nEntries = atoi(nEntriesString.Data());
		nEntries++; // 8191 but actually 8192 lines
		std::cout << "Numer of entries in file: " << nEntries << std::endl;

		// Read bins and initialize the histogram
		TString histName = TString::Format("hist_%s", fileNamePath->Data());
		TH1I* hist = new TH1I(histName, "", nEntries, 0, nEntries);
		for (UInt_t i = 0; i < nEntries; i++){
			std::string sLine = "";
			std::getline(inFile, sLine);
			std::istringstream streamLine(sLine);
			Int_t count;
			streamLine >> count;
			hist->SetBinContent(i+1, count);
		}

		// Read footer
		std::string sLine = "";
		while (getline(inFile, sLine)) {
			TObjString* line = new TObjString(sLine.c_str());
			footer->Add(line);
			std::cout << line->GetString().Data(); // test
		}

		// Find the mean
		Double_t histMaxX = hist->GetXaxis()->GetBinCenter(hist->GetMaximumBin());
		Double_t epsilon = constants.epsilon;
		TString funcName = TString::Format("func_%s", fileNamePath->Data());
		TF1 *func = new TF1(funcName.Data(), "gaus", histMaxX-epsilon, histMaxX+epsilon);
		hist->Fit(func,"SR");
		Double_t par[3];
		func->GetParameters(&par[0]);
		// hist->GetXaxis()->SetRangeUser(histMaxX-epsilon, histMaxX+epsilon);
		// hist->Draw();
		mean = par[1];
	}
};

TList* spectra = new TList();

Int_t ltSpectraShift(const char* fileExt = "Spe", const char* dirPath = ""){
	// Open the directory for scanning
	TSystemDirectory* dir = new TSystemDirectory(dirPath, dirPath);
	TList *files = dir->GetListOfFiles();

	// Exit if directory is empty
	if (!files) {
		std::cout << "Directory \"" << dirPath << "\" is empty." << std::endl;
		return 1;
	}

	// Select files from directory with provided extension
	TIter next(files);
	TSystemFile *file;
	const char* workingDir = gSystem->WorkingDirectory();
	while ((file=(TSystemFile*)next())) {
		TString fName = file->GetName();
		if (!file->IsDirectory() && fName.EndsWith(fileExt)) {
			TString dirPathTString = dirPath;
			const char* absDirPath = gSystem->PrependPathName(workingDir, dirPathTString);
			TString fileName = file->GetName();
			const char* absDirFilePath = gSystem->PrependPathName(absDirPath, fileName);
			Spectrum* spectrum = new Spectrum(absDirFilePath);
			spectra->Add(spectrum);
			// file->Print("V");
		}
	}

	// Exit if no files with certain extension were found
	if (spectra->LastIndex() == 0){
		std::cout << "Directory \"" << dirPath << "\" has no *." << fileExt << " files." << std::endl;
		return 1;
	}

	// Initialize spectra - import file header, histogram and footer
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrum->init();
		}
	}

	// Fit histograms
	return 0;
}

