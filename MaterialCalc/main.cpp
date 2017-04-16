#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <iostream>
#include <cmath>
#include <sstream>
#include <boost/archive/xml_iarchive.hpp>
#include <boost/archive/xml_oarchive.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>

typedef std::map<std::string, int> MatSet;

struct Item
{
    std::string id;
    std::string name;
    std::string method; // workbench, blast_furnace, induction_smelter, etc.
    MatSet materials; // item is considered baasic material if this is empty
    int output = 0;

    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & BOOST_SERIALIZATION_NVP(id);
        ar & BOOST_SERIALIZATION_NVP(name);
        ar & BOOST_SERIALIZATION_NVP(method);
        ar & BOOST_SERIALIZATION_NVP(materials);
        ar & BOOST_SERIALIZATION_NVP(output);
    }
};

std::map<std::string, Item> items;
std::string filename { "material-calculator.xml" };

void restore_data()
{
    std::ifstream ifs(filename);
    if(ifs)
    {
        boost::archive::xml_iarchive ia(ifs);
        ia >> BOOST_SERIALIZATION_NVP(items);
    }
    else
    {
        std::cerr << "cannot read saved data, initialized with empty record." << std::endl;
    }
}

void save_data()
{
    // make an archive
    std::ofstream ofs(filename);
    if(ofs)
    {
        boost::archive::xml_oarchive oa(ofs);
        oa << BOOST_SERIALIZATION_NVP(items);
        std::cout << "data saved." << std::endl;
    }
    else
    {
        std::cerr << "cannot save data." << std::endl;
    }
}

void resolve_dependency(const Item &item, MatSet &accumulation, int amount)
{
    // calc how many unit material is need to build item of amount
    int mat_unit = static_cast<int>(std::ceil(static_cast<float>(amount) / item.output));

    if(item.materials.empty()) // basic material
    {
        accumulation[item.id] += mat_unit;
        return;
    }

    for(auto mat = item.materials.begin(); mat != item.materials.end(); ++mat)
    {
        auto iter = items.find(mat->first);
        if(iter == items.end()) // unknown material
        {
            accumulation[mat->first] += mat->second * mat_unit; // mat->second: how many mat per unit
        }
        else // recursive resolution
        {
            resolve_dependency(iter->second, accumulation, mat_unit * mat->second);
        }
    }
}

std::string query_item_name(const std::string &id)
{
    auto iter = items.find(id);
    if(iter == items.end())
    {
        return "???";
    }
    return iter->second.name;
}

void print_material_set(const Item &item, const MatSet &mat)
{
    std::cout << "\nMaking " << item.name << " with " << item.method << "\n\n#    ID       Name\n------------------------------------\n" << std::left;
    for(auto &&i : mat)
    {
        std::cout << std::setw(4) << i.second << " " << std::setw(8) << i.first << " " << query_item_name(i.first) << std::endl;
    }
    std::cout << std::endl << std::right;
}

void cmd_query(std::stringstream &parser)
{
    std::string id;
    int amount;
    parser >> id >> amount;

    auto iter = items.find(id);
    if(iter == items.end())
    {
        throw std::runtime_error("no such item");
    }

    Item &queried = iter->second;
    std::map<std::string, int> mat;

    resolve_dependency(queried, mat, amount);
    print_material_set(queried, mat);
}

void cmd_add(std::stringstream &parser)
{
    Item building;
    parser >> building.name >> building.id >> building.method;
    std::string id = building.id; // we'll move building

    std::string matid;
    int matnum;

    bool finish = false;
    while(true)
    {
        try
        {
            parser >> matid >> matnum;
        }
        catch(std::runtime_error &)
        {
            if(!finish) throw; return;
        }
        if(matid == "->") // end of decl
        {
            finish = true;
            building.output = matnum; // last number is output num
            auto r = items.insert(std::make_pair(id, std::move(building)));
            if(!r.second)
            {
                throw std::runtime_error("item already exist");
            }
            std::cout << "item added" << std::endl;

            print_material_set(r.first->second, r.first->second.materials);
        }
        else
        {
            building.materials[matid] += matnum;
        }
    }
}

void cmd_remove(std::stringstream &parser)
{
    std::string id;
    parser >> id;

    auto iter = items.find(id);
    if(iter == items.end())
    {
        throw std::runtime_error("no such item");
    }
    items.erase(iter);
    std::cout << "item removed" << std::endl;
}

void show_help()
{
    std::cout
        << "add        a <name> <id> <method> [<mat1Id> <mat1Amount>]... -> <outputNum>\n"
        << "remove     r <id>\n"
        << "calculate  c <id> <amount>\n"
        << "save       s\n"
        << "help       h\n"
        << "save&quit  q\n"
        ;
}

void input_loop()
{
    std::string line, token;
    while(std::cout << "> ", std::getline(std::cin, line))
    {
        try
        {
            std::stringstream parser(line);
            parser.exceptions(std::ios::badbit | std::ios::failbit);
            parser >> token;
            if(token == "c")
            {
                cmd_query(parser);
            }
            else if(token == "a")
            {
                cmd_add(parser);
            }
            else if(token == "r")
            {
                cmd_remove(parser);
            }
            else if(token == "s")
            {
                save_data();
            }
            else if(token == "q")
            {
                return;
            }
            else if(token == "h")
            {
                show_help();
            }
            else
            {
                throw std::runtime_error("no such command");
            }
        }
        catch(std::runtime_error &e)
        {
            std::cerr << e.what() << std::endl;
        }
    }
}

int main()
{
    // query syntax: q <item id> <amount>
    // if not exist:
    // <item id> not defined, define it now? (Y/N) Y -> enter definition mode
    // else:
    // required basic materials:
    // iron: xx
    // copper: xx
    // 3535: xx
    // it appears that 3535 is not defined, so we use:
    // d 3535 wb copper 10 iron 18 <id> <n> -> 10
    // then press enter

    restore_data();
    input_loop();
    save_data();
}
