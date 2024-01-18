#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>

#include "TTree.h"
#include "TFile.h"
#include "ROOT/RDataFrame.hxx"
#include "ROOT/RVec.hxx"
#include <ROOT/RSnapshotOptions.hxx>

#define BRANCH_BUFSIZE 1000000000
#define LOAD_BASKETS_SIZE 2000000000
#define MAX_VIRTUAL_SIZE 2000000000
#define AUTOSAVE_SIZE 2000000000

#define RAW_DATA_FILE_NAME "0111test_tunning01_single.root"

template <typename T>
struct oneLeaf
{
    const std::string name;
    T data;
    int bufsize;
};

struct oneData
{
    oneLeaf<UInt_t> channelID{"channelID", {}, BRANCH_BUFSIZE};
    oneLeaf<Long64_t> time{"time", {}, BRANCH_BUFSIZE};
    oneLeaf<Float_t> energy{"energy", {}, BRANCH_BUFSIZE};
    oneLeaf<Float_t> tot{"tot", {}, BRANCH_BUFSIZE};
};

const std::pair<std::unordered_map<std::string, int>, std::vector<std::string>> getMapping2Detector(const std::string &fileName)
{
    std::ifstream file(fileName);
    std::vector<std::string> keys;
    std::unordered_map<std::string, int> detector2ID;
    if (!file.is_open())
    {
        std::cerr << "Error opening the file: " << fileName << std::endl;
        return {detector2ID, keys}; // Error
    }
    std::string line;

    while (std::getline(file, line))
    {
        std::vector<std::string> row; // save a row of data
        std::stringstream ss(line);
        std::string cell;
        // read each cell divide by ','
        while (std::getline(ss, cell, ','))
        {
            row.push_back(cell);
        }
        const std::string k1 = row[0] + row[1] + row[2] + "u";
        const std::string k2 = row[0] + row[1] + row[2] + "d";
        detector2ID[k1] = std::stoi(row[3]);
        detector2ID[k2] = std::stoi(row[4]);
        keys.push_back(k1);
        keys.push_back(k2);
    }
    file.close();
    std::cout << "Read " << fileName << " finished!" << std::endl;
    return {detector2ID, keys};
}

void printProgressBar(const int currentValue, const int totalValue, const int barWidth = 50)
{
    if (totalValue == 0)
        return;

    double progress = static_cast<double>(currentValue) / totalValue;
    int pos = static_cast<int>(barWidth * progress);

    std::cout << "[";
    for (int i = 0; i < barWidth; ++i)
    {
        if (i < pos)
            std::cout << "=";
        else if (i == pos)
            std::cout << ">";
        else
            std::cout << " ";
    }
    std::cout << "] " << int(progress * 100.0) << " % xi xi\r";
    std::cout.flush();
}

int dataRemake()
{
    // ROOT::EnableImplicitMT();
    std::cout << "Program Started!" << std::endl;

    const auto [detector2ID, order] = getMapping2Detector("Mapping2Detector.csv");
    if (detector2ID.size() == 0)
    {
        return 1;
    }
    std::unordered_map<int, std::string> id2detector;
    for (auto &&pair : std::as_const(detector2ID))
    {
        id2detector[pair.second] = pair.first;
    }

    const std::array<size_t, 2> LPDRange = {256, 512};

    const std::string rawDataFileName = RAW_DATA_FILE_NAME;
    std::unique_ptr<TFile> rawDataFile(TFile::Open(rawDataFileName.c_str(), "READ"));
    // if there's any tree or file disappear, then warn and return empty vector
    if (rawDataFile->IsZombie())
    {
        std::cerr << "Error opening the file: " << rawDataFileName << std::endl;
        return 1;
    }

    const std::string rawDataTreeName = "data";
    const auto rawDataTree = std::unique_ptr<TTree>(static_cast<TTree *>(rawDataFile->Get(rawDataTreeName.c_str())));

    oneData anRawData;
    rawDataTree->SetBranchAddress(anRawData.channelID.name.c_str(), &anRawData.channelID.data);
    rawDataTree->SetBranchAddress(anRawData.time.name.c_str(), &anRawData.time.data);
    rawDataTree->SetBranchAddress(anRawData.energy.name.c_str(), &anRawData.energy.data);
    rawDataTree->SetBranchAddress(anRawData.tot.name.c_str(), &anRawData.tot.data);
    rawDataTree->LoadBaskets(LOAD_BASKETS_SIZE);

    const std::regex regex("([^/]+)(?=(\\.[^/.]+)$)");
    std::smatch match;
    std::regex_search(rawDataFileName, match, regex);
    const auto outputFileName = match.str(1) + " OUTPUT.root";

    std::unique_ptr<TFile> outputFile(TFile::Open(outputFileName.c_str(), "RECREATE"));
    outputFile->cd();

    std::unordered_map<int, std::unique_ptr<TTree>> id2Tree;
    for (int channelID = LPDRange[0]; channelID < LPDRange[1]; channelID++)
    {
        if (id2Tree.find(channelID) == id2Tree.end())
        {
            id2Tree[channelID] = std::make_unique<TTree>(id2detector[channelID].c_str(), id2detector[channelID].c_str());
            id2Tree[channelID]->Branch(anRawData.channelID.name.c_str(), &anRawData.channelID.data, anRawData.channelID.bufsize);
            id2Tree[channelID]->Branch(anRawData.time.name.c_str(), &anRawData.time.data, anRawData.time.bufsize);
            id2Tree[channelID]->Branch(anRawData.energy.name.c_str(), &anRawData.energy.data, anRawData.energy.bufsize);
            id2Tree[channelID]->Branch(anRawData.tot.name.c_str(), &anRawData.tot.data, anRawData.tot.bufsize);
            id2Tree[channelID]->SetMaxVirtualSize(MAX_VIRTUAL_SIZE);
            id2Tree[channelID]->SetAutoSave(AUTOSAVE_SIZE);
        }
    }

    const auto rawDataSize = rawDataTree->GetEntries();
    double progress = 0;
    for (int i = 0; i < rawDataSize; i++)
    {
        rawDataTree->GetEntry(i);
        id2Tree[anRawData.channelID.data]->Fill();
        if (1.0 * i / rawDataSize > progress + .01)
        {
            printProgressBar(i, rawDataSize);
            progress = 1.0 * i / rawDataSize;
        }
    }
    printProgressBar(rawDataSize, rawDataSize);
    std::cout << std::endl;
    std::cout << "Writing remain data to " << outputFileName << std::endl;

    for (auto &&detectorName : std::as_const(order))
    {
        id2Tree.at(detector2ID.at(detectorName))->Write();
    }


    return 0;
}