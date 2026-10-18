#include <iostream>
#include <json.hpp>
#include <fstream>
using json = nlohmann::json;
using namespace std;

int main(int argc, char *argv[])
{
    // Check if a JSON file path is provided as an argument
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <path_to_json_file>" << endl;
        return 1;
    }

    // Read the JSON file
    ifstream jsonFile(argv[1]);
    if (!jsonFile.is_open())
    {
        cerr << "parser: Could not open the file: " << argv[1] << endl;
        return 1;
    }

    // Parse the JSON content
    json jsonData;
    try
    {
        jsonFile >> jsonData;
    }
    catch (const json::parse_error &e)
    {
        cerr << "parser: JSON parse error: " << e.what() << endl;
        return 1;
    }

    if (!jsonData.is_array())
    {
        cerr << "parser: Expected an array of tokens." << endl;
        return 1;
    }

    // Output the parsed JSON data
    cout << "Parsed JSON data:" << endl;
    cout << jsonData.dump(4) << endl;

    return 0;
}
