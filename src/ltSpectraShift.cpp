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
	Double_t channelWidth = 0.006165;
	Int_t minChannel = 0;
	Int_t maxChannel = 0;
 	Int_t keyValueStart = 2;
	Int_t keyValueIncrement = 4;
 	Int_t keyValueCurrent = 0;
	Double_t avgResFuncWidth = 0.3;
	const char* fileExt = "Spe";
	Bool_t flag = kTRUE;
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
	if (bin+intShift >= 1 && bin+intShift <= hist->GetXaxis()->GetNbins()){
		value += (1-ratio)*(Double_t)hist->GetBinContent(bin-intShift);
	}

	// Add contents from the right bin
	if (bin+intShift+1 >= 1 && bin+intShift+1 <= hist->GetXaxis()->GetNbins()){
		value += ratio*(Double_t)hist->GetBinContent(bin-intShift-1);
	}

	return value;
}

// Spectrum - imports itself
class Spectrum : public TObject{
public:
	Spectrum(const char* fNamePath, const char* fName) : TObject() {
		fileName = new TString(fName);
		fileNamePath = new TString(fNamePath);
		// std::cout << fileName->Data() << std::endl;
		header = new TList();
		footer = new TList();
		histogram = new TH1I();
		histogramShifted = new TH1D();
		mean = 0;
		shift = 0;
	}

	const TString* fileName;
	const TString* fileNamePath;
	TList* header;
	TList* footer;
	TH1* histogram;
	TH1* histogramShifted;
	Double_t mean;
	Double_t shift;

	void init(){
		// Open file
		std::cout << fileName->Data() << std::endl; // test
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
		if (constants.flag){
			TCanvas* c = new TCanvas("canvas", "",1024, 600);
			c->SetLogy();
			histogram->Fit(func, "R"); // R = use function range; N - not draw
			gSystem->ProcessEvents();
		}
		else {
			histogram->Fit(func, "RN"); // R = use function range; N - not draw
		}
		Double_t par[3];
		func->GetParameters(&par[0]);

		// Draw first spectrum and ask for chMin, chMax and channelWidth
		if (constants.flag){
			constants.minChannel = histogram->GetXaxis()->GetXmin();
			constants.maxChannel = histogram->GetXaxis()->GetXmax();
			histogram->Draw();
			std::cout << "Input minimum channel (default " << constants.minChannel << "): " << std::endl;
			std::cin >> constants.minChannel;
			std::cout << "Input maximum channel (default " << constants.maxChannel << "): " << std::endl;
			std::cin >> constants.maxChannel;
			std::cout << "Input channel width (default " << constants.channelWidth << "): " << std::endl;
			std::cin >> constants.channelWidth;
			std::cout << "Input start key value (default " << constants.keyValueStart << "): " << std::endl;
			std::cin >> constants.keyValueStart;
			std::cout << "Input key value increment (default " << constants.keyValueIncrement << "): " << std::endl;
			std::cin >> constants.keyValueIncrement;

			constants.keyValueCurrent = constants.keyValueStart;
			constants.flag = kFALSE;
		}
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

	void saveShiftedHistMaestro(const char* shiftedFileNamePath){
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

		fclose(pFile);
	}

	void saveShiftedHistLT(const char* shiftedFileNamePath){
		FILE* pFile = fopen(shiftedFileNamePath, "w");
		if (pFile == NULL) {
			std::cout << "Error writing to file \"" << shiftedFileNamePath << "\"." << std::endl;
		}

		// Write filename
		fprintf(pFile, "%s %d\n", fileName->Data(), constants.keyValueCurrent);

		// Write channel width
		fprintf(pFile, "%f\n", constants.channelWidth);

		// Write key value
		fprintf(pFile, "%d\n", constants.keyValueCurrent);

		// Write default fwhm
		fprintf(pFile, "%.1f\n", constants.avgResFuncWidth);

		// Write tabed histogram data
		Int_t counter = 0;
		for (UInt_t i = 1; i <= histogramShifted->GetXaxis()->GetNbins(); i++){
			// Write channel count
			if (i >= constants.minChannel && i < constants.maxChannel){
				// Write first tab if needed
				if (counter == 0){
					fprintf(pFile, "\t");
				}

				fprintf(pFile, "%f\t", histogramShifted->GetBinContent(i));
				counter++;
			}
			// Reset counter after 10 times
			if (counter == 10){
				counter = 0;
				fprintf(pFile, "\n");
			}
		}
		fclose(pFile);

		// Increment key value for the next spectrum
		constants.keyValueCurrent += constants.keyValueIncrement;
	}
};

TList* spectra = new TList();
Spectrum* spectrumSum;


Int_t ltSpectraShift(const char* dirPath = ""){
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
	TList* filesList = new TList();
	const char* workingDir = gSystem->WorkingDirectory();
	while ((file=(TSystemFile*)next())) {
		TString fName = file->GetName();
		if (!file->IsDirectory() && fName.EndsWith(constants.fileExt)) {
			TObjString* fNameObjString = new TObjString(fName.Data());
			filesList->Add(fNameObjString);
		}
	}

	// Exit if no files with certain extension were found
	if (filesList->LastIndex() == 0){
		std::cout << "Directory \"" << dirPath << "\" has no *." << constants.fileExt << " files." << std::endl;
		return 1;
	}

	// Sort filenames and initialize the spectra
	filesList->Sort(kTRUE);
	for (UInt_t i=0; i <= filesList->LastIndex(); i++){
		TString dirPathTString = dirPath;
		const char* absDirPath = gSystem->PrependPathName(workingDir, dirPathTString);
		TString fileName = ((TObjString*)filesList->At(i))->GetString().Data();
		const char* absDirFilePath = gSystem->PrependPathName(absDirPath, fileName);
		Spectrum* spectrum = new Spectrum(absDirFilePath, ((TObjString*)filesList->At(i))->GetString().Data());
		spectra->Add(spectrum);

		// Initialize sumSpectrum
		if (i==0){
			TString sumFileName = TString::Format("_%s", ((TObjString*)filesList->At(i))->GetString().Data());
			spectrumSum = new Spectrum("", sumFileName.Data());
		}
	}

	// Initialize spectra - import file header, histogram and footer, find mean
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrum->init();

			if (i==0){
				spectrumSum->histogram=new TH1I("sumHist", "sumHist", spectrum->histogram->GetXaxis()->GetNbins(), 0, spectrum->histogram->GetXaxis()->GetNbins());
			}
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

	// Calculate shifts and fill shifted histograms
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrum->shift = spectrum->mean-meanMin;
			spectrum->calcShiftedHist();
		}
	}

	// Sum shifted histograms
	spectrumSum->calcShiftedHist();
	spectrumSum->histogramShifted->Reset();
	for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
		// List file info
		Spectrum* spectrum = (Spectrum*) spectra->At(i);
		if (spectrum){
			spectrumSum->histogramShifted->Add(spectrum->histogramShifted);
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
			TString fN = spectrum->fileName->Data();
			const char* absDirShiftedFilePath = gSystem->PrependPathName(absDirShiftedPath, fN);
			spectrum->saveShiftedHistMaestro(absDirShiftedFilePath);
			TString* absDirShiftedFilePathLT = new TString(absDirShiftedFilePath);
			absDirShiftedFilePathLT->ReplaceAll(".Spe", ".txt");
			spectrum->saveShiftedHistLT(absDirShiftedFilePathLT->Data());
		}
	}

	// Output shifted histogram
	TString sumFileName = spectrumSum->fileName->Data();
	const char* absDirShiftedFileSumPath = gSystem->PrependPathName(absDirShiftedPath, sumFileName);
	TString* absDirShiftedFileSumPathLT = new TString(absDirShiftedFileSumPath);
	absDirShiftedFileSumPathLT->ReplaceAll(".Spe", ".txt");
	spectrumSum->saveShiftedHistLT(absDirShiftedFileSumPathLT->Data());

	// Output means
	TString meansFilename = "mean-values.txt";
	const char* meansFilePathName = gSystem->PrependPathName(absDirShiftedPath, meansFilename);
	FILE* pFile = fopen(meansFilePathName, "w");
	if (pFile == NULL){
		std::cout << "Error writing to file \"" << meansFilePathName << "\"." << std::endl;
	}
	else {
		fprintf(pFile, "\"Filename\"\t\"mean\"\t\"shift\"\n");
		for (UInt_t i = 0; i <= spectra->LastIndex(); i++){
			Spectrum* spectrum = (Spectrum*) spectra->At(i);
			if (spectrum){
				fprintf(pFile, "%s\t%f\t%f\n", spectrum->fileName->Data(), spectrum->mean, spectrum->shift);
			}
		}
		fclose(pFile);
	}

	// Exit
	return 0;
}

