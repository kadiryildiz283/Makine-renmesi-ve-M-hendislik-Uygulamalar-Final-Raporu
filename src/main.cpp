#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <curl/curl.h>
#include <nlohmann/json.hpp> 
#include <unordered_map>
#include <set>

using json = nlohmann::json;

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

std::string cleanHtml(const std::string& input) {
    std::string cleaned = input;

    // HTML yorumlarını temizlendi
    cleaned = std::regex_replace(cleaned, std::regex("<!--.*?-->"), "");

    // Fazla boşlukları temizlendi
    cleaned = std::regex_replace(cleaned, std::regex("\\s+"), " ");

    // Dolar ve boşluklar çıkartıldı
    cleaned = std::regex_replace(cleaned, std::regex("^\\s+|\\s+$"), "");

    return cleaned;
}

int main() {
    CURL* curl;
    CURLcode res;

    // Title'ları anahtar olarak kullanan bir map
    std::unordered_map<std::string, std::set<json>> groupedData;

    for (int page = 2; page <= 100; ++page) {
        // HTML içeriğini saklamak için string
        std::string htmlContent;

        // CURL'i başlat
        curl = curl_easy_init();
        if (curl) {
            // URL'yi ayarla
            std::string url = "https://www.trendyol.com/erkek-t-shirt-x-g2-c73?pi=" + std::to_string(page);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            // Redirect'leri takip et
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            // Callback fonksiyonunu ayarla
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

            // Callback fonksiyonuna aktarılacak veriyi ayarla
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &htmlContent);

            // İsteği gerçekleştir
            res = curl_easy_perform(curl);

            if (res != CURLE_OK) {
                std::cerr << "CURL hatası: " << curl_easy_strerror(res) << std::endl;
                curl_easy_cleanup(curl);
                continue;
            }

            // Regex ile verileri eşleştir
            std::regex divWithTitleDiscountAndRatingRegex(
                "<div[^>]*class=\\\"p-card-wrppr with-campaign-view[^\\\"]*\\\"[^>]*title=\\\"([^\\\"]*)\\\"[^>]*>.*?"
                "<span[^>]*class=\\\"[^\\\"]*rating-score[^\\\"]*\\\"[^>]*>(.*?)</span>"
            );
            std::regex divWithTitleDiscountAndRatingRegex2(
                "<div[^>]*class=\\\"p-card-wrppr with-campaign-view[^\\\"]*\\\"[^>]*title=\\\"([^\\\"]*)\\\"[^>]*>.*?"
                "<div[^>]*class=\\\"prc-box-dscntd[^\\\"]*\\\"[^>]*>(.*?)</div>.*?"
            );
            std::regex divWithTitleDiscountAndRatingRegex3(
                "<div[^>]*class=\\\"p-card-wrppr with-campaign-view[^\\\"]*\\\"[^>]*title=\\\"([^\\\"]*)\\\"[^>]*>.*?"
                "<span[^>]*class=\\\"ratingCount[^\\\"]*\\\"[^>]*>(.*?)</span>.*?"
            );
            std::regex divWithTitleDiscountAndRatingRegex4(
                "<div[^>]*class=\\\"p-card-wrppr with-campaign-view[^\\\"]*\\\"[^>]*title=\\\"([^\\\"]*)\\\"[^>]*>.*?"
                "<span[^>]*class=\\\"focused-text[^\\\"]*\\\"[^>]*>(.*?)</span>.*?"
            );

            std::smatch match;

            // İlk regex ile eşleşen veriler
            std::string::const_iterator searchStart(htmlContent.cbegin());
            while (std::regex_search(searchStart, htmlContent.cend(), match, divWithTitleDiscountAndRatingRegex)) {
                std::string title = cleanHtml(match[1]);
                json item;
                item["rating"] = cleanHtml(match[2]);
                groupedData[title].insert(item);
                searchStart = match.suffix().first;
            }

            // Diğer regex'ler için işlemleri tekrarla
            searchStart = htmlContent.cbegin();
            while (std::regex_search(searchStart, htmlContent.cend(), match, divWithTitleDiscountAndRatingRegex2)) {
                std::string title = cleanHtml(match[1]);
                json item;
                item["discounted_price"] = cleanHtml(match[2]);
                groupedData[title].insert(item);
                searchStart = match.suffix().first;
            }

            searchStart = htmlContent.cbegin();
            while (std::regex_search(searchStart, htmlContent.cend(), match, divWithTitleDiscountAndRatingRegex3)) {
                std::string title = cleanHtml(match[1]);
                json item;
                item["review_count"] = cleanHtml(match[2]);
                groupedData[title].insert(item);
                searchStart = match.suffix().first;
            }

            searchStart = htmlContent.cbegin();
            while (std::regex_search(searchStart, htmlContent.cend(), match, divWithTitleDiscountAndRatingRegex4)) {
                std::string title = cleanHtml(match[1]);
                json item;
                item["favorites"] = cleanHtml(match[2]);
                groupedData[title].insert(item);
                searchStart = match.suffix().first;
            }

            // CURL'i temizle
            curl_easy_cleanup(curl);
        } else {
            std::cerr << "CURL başlatılamadı!" << std::endl;
        }
    }

    // Verileri JSON formatına dönüştür:
    json result;
    for (const auto& [title, items] : groupedData) {
        result[title] = json::array();
        for (const auto& item : items) {
            result[title].push_back(item);
        }
    }

    // JSON verilerini bir dosyaya kaydet
    std::ofstream file("output.json");
    if (file.is_open()) {
        file << result.dump(4); // 4 boşluklu biçimlendirme
        file.close();
        std::cout << "Veriler toplandı." << std::endl;
    } else {
        std::cerr << "Neden olmadı bende anlamadım!" << std::endl;
    }

    return 0;
}

