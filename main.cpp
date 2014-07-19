#include <iostream>
#include <string>
#include <fstream>
#include <cstring>
#include <vector>
#include <map>
#include <cstdlib>

#include <tidylib/include/tidy/tidy.h>
#include <tidylib/include/tidy/buffio.h>

#include <libxml2/include/libxml/parser.h>
#include <libxml2/include/libxml/tree.h>

#include <curl/include/curl/curl.h>

#include <libxl/include_cpp/libxl.h>

using namespace libxl;
using namespace std;

int level;
Book *book;
Sheet *sheet;
int parent;
int sno;
string xpath;
map<string, int> content;

static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string Request(string url)
{
    CURL *curl;
    CURLcode res;
    string result;

    curl = curl_easy_init();
    if(curl)
    {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback); // Passing the function pointer to LC
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result); // Passing our BufferStruct to LC
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
          fprintf(stderr, "curl_easy_perform() failed: %s\n",
                  curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }
    return result;
}

string CleanHTML(const std::string &html){
    // Initialize a Tidy document
    TidyDoc tidyDoc = tidyCreate();
    TidyBuffer tidyOutputBuffer = {0};

    // Configure Tidy
    // The flags tell Tidy to output XML and disable showing warnings
    bool configSuccess = tidyOptSetBool(tidyDoc, TidyXmlOut, yes)
        && tidyOptSetBool(tidyDoc, TidyQuiet, yes)
        && tidyOptSetBool(tidyDoc, TidyNumEntities, yes)
        && tidyOptSetBool(tidyDoc, TidyShowWarnings, no);

    int tidyResponseCode = -1;

    // Parse input
    if (configSuccess)
        tidyResponseCode = tidyParseString(tidyDoc, html.c_str());

    // Process HTML
    if (tidyResponseCode >= 0)
        tidyResponseCode = tidyCleanAndRepair(tidyDoc);

    // Output the HTML to our buffer
    if (tidyResponseCode >= 0)
        tidyResponseCode = tidySaveBuffer(tidyDoc, &tidyOutputBuffer);

    // Any errors from Tidy?
    if (tidyResponseCode < 0)
        throw ("Tidy encountered an error while parsing an HTML response. Tidy response code: " + tidyResponseCode);

    // Grab the result from the buffer and then free Tidy's memory
    std::string tidyResult = (char*)tidyOutputBuffer.bp;
    tidyBufFree(&tidyOutputBuffer);
    tidyRelease(tidyDoc);

    return tidyResult;
}
bool IsNewline(char c)
{
    if(c == '\n')
        return true;
    return false;
}
bool PrintElementNames(xmlNode *node)
{
    map<string, int> xp;
    xmlNode *cur_node = NULL;
    for(cur_node = node; cur_node; cur_node = cur_node->next)
    {
        /*if(cur_node->type == XML_ELEMEstring orig_xpath = xpath;
            xpath.append("//");NT_NODE)
        {
            re << "node type: Element, name: " << cur_node->name << endl;
        }*/
        string orig_xpath = xpath;
        xpath.append("//");

        string temp = (const char *)cur_node->name;

        xpath.append(temp);
        if(xp.find(temp) != xp.end())
        {
            xp[temp]++;
            char te[1000];
            itoa(xp[temp], te, 10);
            xpath.append("[");
            xpath.append(te);
            xpath.append("]");
        }
        else
            xp[temp] = 1;
        if(strcmp((const char *)cur_node->name, "head") != 0)
        {
            level++;
            PrintElementNames(cur_node->children);
            level--;
        }
        if(cur_node->type == 1)
        {
            if(xmlNodeGetContent(cur_node)[0] != '\0' && strcmp(
                (const char *)cur_node->name, "style") != 0 &&
               strcmp((const char *)cur_node->name, "script") != 0 &&
               strcmp((const char *)cur_node->name, "head") != 0 &&
               strcmp((const char *)cur_node->name, "li") != 0 &&
               strcmp((const char *)cur_node->name, "div") &&
               strcmp((const char *)cur_node->name, "head") != 0 &&
               strcmp((const char *)cur_node->name, "ol") != 0 &&
               strcmp((const char *)cur_node->name, "center") != 0 &&
               strcmp((const char *)cur_node->name, "tr") != 0 &&
               strcmp((const char *)cur_node->name, "td") != 0 &&
               strcmp((const char *)cur_node->name, "html") != 0 &&
               strcmp((const char *)cur_node->name, "font") != 0 &&
               strcmp((const char *)cur_node->name, "form") != 0 &&
               strcmp((const char *)cur_node->name, "body") != 0 &&
               strcmp((const char *)cur_node->name, "ul") != 0)
            {
                /*re << xmlNodeGetContent(cur_node) << endl;
                re << "node type: " << cur_node->type << " name: " << cur_node->name << endl;
                re << "children: " << cur_node->children << endl;
                re << level << endl;*/
                //cout << cur_node->nsDef << endl;
                sheet->writeNum(sno + 1, 0, sno);
                string cont = (const char *)xmlNodeGetContent(cur_node);
                cont.erase(remove_if(cont.begin(), cont.end(), &IsNewline), cont.end());
                //remove(cont.begin(), cont.end(), '\a');
                if(content.find(cont) != content.end())
                {
                    char te[1000];
                    itoa(content[cont], te, 10);
                    content[cont]++;
                    cont += "_";
                    cont += te;
                }
                else
                {
                    content[cont] = 1;
                }
                sheet->writeStr(sno + 1, 1, cont.c_str());
                sheet->writeNum(sno + 1, 3, parent);
                sheet->writeStr(sno + 1, 4, xpath.c_str());
                sno++;
            }
        }
        xpath = orig_xpath;
    }
    return true;
}

void ParseXML(const string &xml)
{
    xmlDoc *doc = NULL;
    xmlNode *root_element = NULL;
    doc = xmlReadMemory(xml.c_str(), xml.length(), "noname.xml", NULL, 0);
    if(doc == NULL)
    {
         printf("error: could not parse file\n");
    }
    root_element = xmlDocGetRootElement(doc);
    PrintElementNames(root_element);
    xmlFreeDoc(doc);
    xmlCleanupParser();
    xmlMemoryDump();
}

int main()
{
    string file;
    cout << "Enter filename of file containing URLs (example: URL.txt): ";
    cin >> file;
    ifstream in(file.c_str());string orig_xpath = xpath;
            xpath.append("//");
    book = xlCreateBook();
    if(!book)
    {
        cout << "Error creating XLS!" << endl;
        return 1;
    }
    sheet = book->addSheet("Sheet1");
    if(!sheet)
    {
        cout << "Error creating XLS!" << endl;
        return 1;
    }
    sheet->writeStr(1, 0, "Sno");
    sheet->writeStr(1, 1, "Object Name");
    sheet->writeStr(1, 2, "Object Type");
    sheet->writeStr(1, 3, "Parent");
    sheet->writeStr(1, 4, "ObjectPath");
    sno = 1;
    int orig = 1;
    while(!in.eof())
    {
        content.clear();
        xpath = "xpath=";
        string url;
        in >> url;
        cout << "Connecting to " << url << ":" << endl;
        string html = Request(url);
        cout << "Converting HTML to XML... ";
        string result = CleanHTML(html);
        parent = sno;
        sheet->writeNum(sno + 1, 0, sno);
        sheet->writeStr(sno + 1, 1, url.c_str());
        sheet->writeStr(sno + 1, 2, "Page");
        sheet->writeNum(sno + 1, 3, 0);
        sheet->writeStr(sno + 1, 4, "name=");
        sno++;
        cout << "Parsing XML... " ;
        map<string, int> temp;
        ParseXML(result);
        int items = sno - orig + 1;
        orig = sno;
        cout << items << " items." << endl;
        cout << endl;
    }
    book->save("result.xls");
    book->release();
    return 0;
}
