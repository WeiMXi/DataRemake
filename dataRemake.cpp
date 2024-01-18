#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <regex>

#include "TTree.h"
#include "TFile.h"
#include "TStopwatch.h"
#include "TString.h"

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
    const int bufsize;
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

int main()
{
    // ROOT::EnableImplicitMT();
    std::cout << "Program Started!" << std::endl;
    TStopwatch timer;
    timer.Start();

    const auto [detector2ID, order] = getMapping2Detector("Mapping2Detector.csv");
    if (detector2ID.size() == 0)
    {
        return 1;
    }
    const auto reverseMap = [&]()
    {
        std::unordered_map<int, std::string> map;
        for (auto &&pair : std::as_const(detector2ID))
        {
            map[pair.second] = pair.first;
        }
        return map;
    };
    const auto id2detector = reverseMap();

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

    oneData aRawData;
    rawDataTree->SetBranchAddress(aRawData.channelID.name.c_str(), &aRawData.channelID.data);
    rawDataTree->SetBranchAddress(aRawData.time.name.c_str(), &aRawData.time.data);
    rawDataTree->SetBranchAddress(aRawData.energy.name.c_str(), &aRawData.energy.data);
    rawDataTree->SetBranchAddress(aRawData.tot.name.c_str(), &aRawData.tot.data);
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
            id2Tree[channelID] = std::make_unique<TTree>(id2detector.at(channelID).c_str(), id2detector.at(channelID).c_str());
            id2Tree[channelID]->Branch(aRawData.channelID.name.c_str(), &aRawData.channelID.data, aRawData.channelID.bufsize);
            id2Tree[channelID]->Branch(aRawData.time.name.c_str(), &aRawData.time.data, aRawData.time.bufsize);
            id2Tree[channelID]->Branch(aRawData.energy.name.c_str(), &aRawData.energy.data, aRawData.energy.bufsize);
            id2Tree[channelID]->Branch(aRawData.tot.name.c_str(), &aRawData.tot.data, aRawData.tot.bufsize);
            id2Tree[channelID]->SetMaxVirtualSize(MAX_VIRTUAL_SIZE);
            id2Tree[channelID]->SetAutoSave(AUTOSAVE_SIZE);
        }
    }

    const auto rawDataSize = rawDataTree->GetEntries();
    double progress = 0;
    for (int i = 0; i < rawDataSize; i++)
    {
        rawDataTree->GetEntry(i);
        id2Tree[aRawData.channelID.data]->Fill();
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

    // timer, stop and print
    timer.Stop();
    const auto realTime = timer.RealTime();
    const auto cpuTime = timer.CpuTime();
    std::cout << Form("RealTime: %4.2f s, CpuTime: %4.2f s", realTime, cpuTime) << std::endl;

    return 0;
}