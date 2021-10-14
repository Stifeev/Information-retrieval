# -*- coding: utf-8 -*-

from scrapy.spiders import Spider

from film_scraper.items import FilmScraperItem
from film_scraper.settings import PATH_2_FILMS_JSON, PATH_2_SITEMAP, PATH_2_DRIVER, PATH_2_CAPTCHA, \
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

def fetch_reviews(spider, item, url, review_count):
    item["reviews"] = [""] * review_count
    
    url_sheme = urljoin(re.sub("/series/", "/film/", url), "reviews/ord/date/status/all/perpage/{:d}/page/{:d}/")
    k = 0
    while k * REVIEWS_OFFSET < review_count:
        time.sleep(DOWNLOAD_DELAY)
        
        spider.logger.info("Getting {:d}-reviews page".format(k + 1))
        spider.driver.get(url_sheme.format(REVIEWS_OFFSET, k + 1))
        
        if "captcha" in spider.driver.current_url.lower():
            spider.logger.warning("Skip {:s}".format(url))
            spider.captcha_fp.write(url + '\n')
            spider.n_captchas += 1
            return False
            
        spider.logger.info("Success getting {:d}-rewiews page".format(k + 1))
        
        page = BeautifulSoup(spider.driver.page_source, "lxml")
        
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
    n_captchas = 0
    n_links = 0
    n_stores = 0
    current_hour = 0
    
    captcha_fp = None
    feed_fp = None
    
    def __init__(self, *args, **kwargs):
        super(FSpider, self).__init__(*args, **kwargs)
        
        self.current_hour = datetime.now().hour
        
        self.logger.info("Start extracting keys")
        
        if os.path.exists(PATH_2_FILMS_JSON):
            with jsonlines.open(PATH_2_FILMS_JSON, mode='r') as fp:
                for item in fp:
                    if not "page_url" in item:
                        continue
                    key = extract_key(item["page_url"])
                    self.film_ids |= set([key])
        
        self.logger.info("Finish extracting keys")
        
        self.logger.info("Start extracting links")
        
        if os.path.exists(PATH_2_SITEMAP):
            with open(PATH_2_SITEMAP, "r") as fp:
                print("OK")
                # Пропуск ссылок
                for i in range(1, IBEGIN):
                    fp.readline()
                
                # Добавление нужных
                for i in range(IBEGIN, IEND + 1):
                    link = fp.readline().strip()
                    key = extract_key(link)
                    
                    if not key in self.film_ids:
                        self.start_urls.append(link)
         
        print(self.start_urls)
        self.n_links = len(self.start_urls)
        self.logger.info("Finishing extracting links")
        
        if os.path.exists(PATH_2_CAPTCHA):
            os.remove(PATH_2_CAPTCHA)
            
        self.captcha_fp = open(PATH_2_CAPTCHA, "w")
        self.feed_fp = jsonlines.open(PATH_2_FILMS_JSON, "a")
        
        chrome_options = Options()
        chrome_options.add_argument('--headless')
        #chrome_options.add_argument("--proxyserver=socks5://127.0.0.1:9150")
        self.driver =  webdriver.Chrome(executable_path=PATH_2_DRIVER,
                                        options=chrome_options)

    def parse(self, response):
        print("Success")
        
        
        hour = datetime.now().hour
        if self.current_hour < hour or self.current_hour == 23 and hour == 0:
            self.current_hour = hour
            
            # Отдыхаем
            self.logger.info("Start sleeping for {:d} min".format(MINUTES_SLEEP_PER_HOUR))
            time.sleep(MINUTES_SLEEP_PER_HOUR * 60)
            self.logger.info("Back to work")
        
        self.i_link += 1
        self.logger.info("Processing link {:d}/{:d}. Extracting data from {:s}".format(self.i_link, 
                                                                                       self.n_links,
                                                                                       response.url))
        if "captcha" in response.url.lower():
            self.logger.warning("Skip {:s}".format(response.url))
            self.captcha_fp.write(response.url + '\n')
            self.n_captchas += 1
            return
        
        item = FilmScraperItem()
        item["page_url"] = response.url
        
        page = BeautifulSoup(response.text, "lxml")
        
        review_count = fetch_headers(page, item)
        self.logger.info("Total reviews = {:d}".format(review_count))
        
        if fetch_reviews(self, item, response.url, review_count):
            self.logger.info(item)
            self.feed_fp.write(item._values)
            self.n_stores += 1
        else:
            return

    def closed(self, reason):
        self.driver.close()
        
        self.captcha_fp.close()
        self.feed_fp.close()
        
        self.logger.info("Stores: {:d}, Captcha links: {:d}".format(self.n_stores, 
                                                                    self.n_captchas))
        
        