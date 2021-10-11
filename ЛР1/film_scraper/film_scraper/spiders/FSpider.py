# -*- coding: utf-8 -*-

from scrapy.spiders import Spider

from film_scraper.items import FilmScraperItem
from film_scraper.settings import PATH_2_JSON, PATH_2_LINKS, PATH_2_DRIVER, PATH_2_CAPTCHA_LINKS, \
                                  IBEGIN, IEND, DOWNLOAD_DELAY, REVIEWS_OFFSET, MINUTES_SLEEP_PER_HOUR

from selenium import webdriver
from selenium.webdriver.chrome.options import Options

from bs4 import BeautifulSoup
import re

import os
import time
import jsonlines

from urllib.parse import urljoin

from datetime import datetime

def extract_key(url):
    keys = url.split('/')
    while "" in keys:
        keys.remove("")
    return int(keys[-1])

def find_wrap(search):
    return search.text if search else ""

def fetch_headers(page, item):
    item["title"] = find_wrap(page.find("h1", 
                                            { "class": re.compile("^styles_title"),
                                              "itemprop": "name" }).span)
        
    item["alternative_title"] = find_wrap(page.find("span", 
                                                    { "class": re.compile("^styles_originalTitle") }))
    
    year = page.find("div", 
                      class_=re.compile("^styles"),
                      text=re.compile(" *Год производства *"))
    
    if year and year.next_sibling and year.next_sibling.a:
        item["year"] = year.next_sibling.a.text
    
    countries = page.find("div",
                      class_=re.compile("^styles"),
                      text=re.compile(" *Страна *"))
    if countries and countries.next_sibling:
        item["countries"] = find_wrap(countries.next_sibling)
    
    directors = page.find("div",
                          class_=re.compile("^styles"),
                          text=re.compile(" *Режисс(е|ё)р *"))
    if directors and directors.next_sibling:
        item["directors"] = find_wrap(directors.next_sibling)
        
    screenwriters = page.find("div",
                              class_=re.compile("^styles"),
                              text=re.compile(" *Сценарий *"))
    if screenwriters and screenwriters.next_sibling:
        item["screenwriters"] = find_wrap(screenwriters.next_sibling)
        
    genres = page.find("div",
                       class_=re.compile("^styles"),
                       text=re.compile(" *Жанр *"))
    if genres and genres.next_sibling:
        item["genres"] = find_wrap(genres.next_sibling)
    if item["genres"].endswith("слова"):
        item["genres"] = item["genres"][:-5]
    
    if page.find("a", class_=re.compile("^styles_link"), text=re.compile(" *В главных ролях *")):
        item["actors"] = []
        actors = page.find("ul", class_=re.compile("^styles_list"))
        for actor in actors.children:
            item["actors"].append(actor.text)
    
    item["score"] = find_wrap(page.find("a", class_="film-rating-value"))
    item["description"] = find_wrap(page.find("p", class_=re.compile("^styles_paragraph")))
    
    review_count = re.match("[0-9]+", find_wrap(page.find("div", class_=re.compile("reviewCount"))))
    return int(review_count.group(0)) if review_count else 0

def fetch_reviews(item, driver, url, review_count, logger, captcha_fp):
    item["reviews"] = [""] * review_count
    
    url_sheme = urljoin(re.sub("/series/", "/film/", url), "reviews/ord/date/status/all/perpage/{:d}/page/{:d}/")
    k = 0
    while k * REVIEWS_OFFSET < review_count:
        time.sleep(DOWNLOAD_DELAY)
        
        logger.info("Getting {:d}-reviews page".format(k + 1))
        driver.get(url_sheme.format(REVIEWS_OFFSET, k + 1))
        
        if "captcha" in driver.current_url.lower():
            logger.info("Fail to get {:d}-reviews page because of captcha".format(k + 1))
            captcha_fp.write(url_sheme.format(REVIEWS_OFFSET, k + 1) + '\n')
            return False
            
        logger.info("Success getting {:d}-rewiews page".format(k + 1))
        
        page = BeautifulSoup(driver.page_source, "lxml")
        
        reviews_block = page.find_all("div", class_="reviewItem userReview")
        
        for i, review_block in enumerate(reviews_block):
            item["reviews"][k * REVIEWS_OFFSET + i] += find_wrap(review_block.find("p", class_="sub_title")) + '\n'
            item["reviews"][k * REVIEWS_OFFSET + i] += find_wrap(review_block.find("span", itemprop="reviewBody"))
        
        k += 1
    
    while "" in item["reviews"]:
        item["reviews"].remove("")
        
    return True

class FSpider(Spider):
    name = 'FSpyder'
    allowed_domains = ["www.kinopoisk.ru"]
    start_urls = []
    
    film_ids = set()
    
    i_link = 0
    n_captcha = 0
    n_link = 0
    current_hour = 0
    
    captcha_fp = None
    
    def __init__(self, *args, **kwargs):
        super(FSpider, self).__init__(*args, **kwargs)
        
        self.current_hour = datetime.now().hour
        
        self.logger.info("Start extracting keys")
        
        if os.path.exists(PATH_2_JSON):
            with jsonlines.open(PATH_2_JSON, mode='r') as fp:
                for item in fp:
                    if not "page_url" in item:
                        continue
                    key = extract_key(item["page_url"])
                    self.film_ids |= set([key])
        
        self.logger.info("Finish extracting keys")
        
        self.logger.info("Start extracting links")
        
        if os.path.exists(PATH_2_LINKS):
            with open(PATH_2_LINKS, "r") as fp:
                
                # Пропуск ссылок
                for i in range(1, IBEGIN):
                    fp.readline()
                
                # Добавление нужных
                for i in range(IBEGIN, IEND + 1):
                    link = fp.readline().strip()
                    key = extract_key(link)
                    
                    if not key in self.film_ids:
                        self.start_urls.append(link)
                    
        self.n_link = len(self.start_urls)
        # self.logger.info(str(self.start_urls))
        self.logger.info("Finishing extracting links")
        
        if os.path.exists(PATH_2_CAPTCHA_LINKS):
            os.remove(PATH_2_CAPTCHA_LINKS)
            
        self.captcha_fp = open(PATH_2_CAPTCHA_LINKS, "a")
        
        chrome_options = Options()
        chrome_options.add_argument('--headless')
        #chrome_options.add_argument("--proxyserver=socks5://127.0.0.1:9150")
        self.driver =  webdriver.Chrome(executable_path=PATH_2_DRIVER,
                                        options=chrome_options)

    def parse(self, response):
        
        hour = datetime.now().hour
        if self.current_hour < hour or self.current_hour == 23 and hour == 0:
            self.current_hour = hour
            
            # Отдыхаем
            self.logger.info("Start sleeping for {:d} min".format(MINUTES_SLEEP_PER_HOUR))
            time.sleep(MINUTES_SLEEP_PER_HOUR * 60)
            self.logger.info("Back to work")
        
        self.i_link += 1
        self.logger.info("Processing link {:d}/{:d}. Extracting data from {:s}".format(self.i_link, 
                                                                                       self.n_link,
                                                                                       response.url))
        if "captcha" in response.url.lower():
            self.logger.info("Skip {:s}".format(response.url))
            self.captcha_fp.write(response.url + '\n')
            self.n_captcha += 1
            return
        
        item = FilmScraperItem()
        item["page_url"] = response.url
        
        page = BeautifulSoup(response.text, "lxml")
        
        review_count = fetch_headers(page, item)
        self.logger.info("Total reviews = {:d}".format(review_count))
        
        if fetch_reviews(item, self.driver, response.url, review_count, self.logger, self.captcha_fp):
            return item
        else:
            return

    def closed(self, reason):
        self.driver.close()
        self.captcha_fp.close()
        
        self.logger.info("Captcha links: {:d}".format(self.n_captcha))
        self.logger.info("Removing empty items")
        name, ext = os.path.splitext(PATH_2_JSON)
        
        PATH_2_JSON_TMP = name + "_tmp" + ext
        
        with jsonlines.open(PATH_2_JSON, mode='r') as fp1:
            with jsonlines.open(PATH_2_JSON_TMP, mode='w') as fp2:
                for item in fp1:
                    if item != {}:
                        fp2.write(item)
                        
        os.remove(PATH_2_JSON)
        os.rename(PATH_2_JSON_TMP, PATH_2_JSON)
        