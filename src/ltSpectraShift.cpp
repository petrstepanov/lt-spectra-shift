// Shifting spectra functions
// Petr Stepanov stepanovps@gmail.com

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
#include <stdio.h>
#include <TObjString.h>

// Constants
struct Constants {
	const UInt_t headerLines = 12; // header lines number
	const Int_t epsilon = 10; // epsilon-bins around the maimum to fit
	TString shiftedFolderName = "shifted";
};
Constants constants;

Double_t getBinContentShifted(TH1* hist, Int_t bin, Double_t shift){
	if (shift == 0){
		return hist->GetBinContent(bin);
	}

	Double_t value = 0;
	Int_t intShift = (Int_t) shift;
	Double_t ratio = shift - (Double_t)intShift;

	// Add contents from the left bin
	if (bin-intShift-1 >= 1 && bin-intShift-1 <= hist->GetXaxis()->GetNbins()){
		value += ratio*(Double_t)hist->GetBinContent(bin-intShift-1);
	}

	// Add contents from the right bin
	if (bin-intShift >= 1 && bin-intShift <= hist->GetXaxis()->GetNbins()){
		value += (1-ratio)*(Double_t)hist->GetBinContent(bin-intShift);
	}
	return value;
}

// Spectrum - imports itself
class Spectrum : public TObject{
public:
	Spectrum(const char* fNamePath, const char* fName) : TObject() {
		fileName = new TString(fName);
		fileNamePath = new TString(fNamePath);
		header = new TList();
		footer = new TList();
		histogram = new TH1I();
		histogramShifted = new TH1D();
		mean = 0;
		shift = 0;
	}

	TString* fileName;
	TString* fileNamePath;
	TList* header;
	TList* footer;
	TH1* histogram;
	TH1* histogramShifted;
	Double_t mean;
	Double_t shift;

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
		TString histName = TString::Format("hist_%s", fileName->Data());
		histogram = new TH1I(histName.Data(), histName.Data(), nEntries, 0, nEntries);
		for (UInt_t i = 0; i < nEntries; i++){
			std::string sLine = "";
			std::getline(inFile, sLine);
			std::istringstream streamLine(sLine);
			Int_t count;
			streamLine >> count;
			histogram->SetBinContent(i+1, count);
		}

		// Read footer
		std::string sLine = "";
		while (getline(inFile, sLine)) {
			TObjString* line = new TObjString(sLine.c_str());
			footer->Add(line);
			std::cout << line->GetString().Data(); // test
		}

		// Find the mean
		Double_t histMaxX = histogram->GetXaxis()->GetBinCenter(histogram->GetMaximumBin());
		Double_t epsilon = constants.epsilon;
		TString funcName = TString::Format("func_%s", fileName->Data());
		TF1 *func = new TF1(funcName.Data(), "gaus", histMaxX-epsilon, histMaxX+epsilon);
		histogram->Fit(func,"RN"); // R = use function range; N - not draw
		Double_t par[3];
		func->GetParameters(&par[0]);
		// hist->GetXaxis()->SetRangeUser(histMaxX-epsilon, histMaxX+epsilon);
		// hist->Draw();
		mean = par[1];
	}

	void calcShiftedHist(){
		TString histShiftedName = TString::Format("%s_shifted", histogram->GetName());
		histogramShifted = new TH1D(histShiftedName.Data(), "", histogram->GetXaxis()->GetNbins(), 0, histogram->GetXaxis()->GetNbins());
		for (UInt_t i=1; i<=histogramShifted->GetXaxis()->GetNbins(); i++){
			Double_t shiftBinContent = getBinContentShifted(histogram, i, shift);
			histogramShifted->SetBinContent(i, shiftBinContent);
		}
	}

	void saveShiftedHist(const char* shiftedFileNamePath){
		FILE* pFile = fopen(shiftedFileNamePath, "w");
		if (pFile == NULL) {
			std::cout << "Error writing to file \"" << shiftedFileNamePath << "\"." << std::endl;
		}

		// Write header
		for (UInt_t i=0; i<=header->LastIndex(); i++){
			TObjString* objString = (TObjString*)header->At(i);
			fprintf(pFile, "%s\n", objString->GetString().Data());
		}

		// Write histogram
		for (UInt_t i=1; i<=histogramShifted->GetXaxis()->GetNbins(); i++){
			fprintf(pFile, "       %f\n", histogramShifted->GetBinContent(i));
		}

		// Write footer
		for (UInt_t i=0; i<=footer->LastIndex(); i++){
			TObjString* objString = (TObjString*)footer->At(i);
			fprintf(pFile, "%s\n", objString->GetString().Data());
		}
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
			Spectrum* spectrum = new Spectrum(absDirFilePath, file->GetName());
			spectra->Add(spectrum);
			// file->Print("V");
		}
	}

	// Exit if no files with certain extension were found
	if (spectra->LastIndex() == 0){
		std::cout << "Directory \"" << dirPath << "\" has no *." << fileExt << " files." << std::endl;
		return 1;
	}

	// Initialize spectra - import file header, histogram and footer, find mean
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrum->init();
		}
	}

	// Find the minimum mean
	Double_t meanMin = ((Spectrum*)spectra->At(0))->mean;
	std::cout << meanMin << std:: endl;
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			if (spectrum->mean < meanMin) meanMin = spectrum->mean;
		}
	}

	// Calculate shifts and export shifted histogram
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrum->shift = spectrum->mean-meanMin;
			spectrum->calcShiftedHist();
		}
	}

	// Create output directory
	TString dirPathTString = dirPath;
	const char* absDirPath = gSystem->PrependPathName(workingDir, dirPathTString);
	const char* absDirShiftedPath = gSystem->PrependPathName(absDirPath, constants.shiftedFolderName);
	gSystem->mkdir(absDirShiftedPath);

	// Write shifted spectra
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			const char* absDirShiftedFilePath = gSystem->PrependPathName(absDirShiftedPath, *(spectrum->fileName));
			spectrum->saveShiftedHist(absDirShiftedFilePath);
		}
	}

	return 0;
}

