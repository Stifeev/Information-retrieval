# Scrapy settings for film_scraper project
#
# For simplicity, this file contains only settings considered important or
# commonly used. You can find more settings consulting the documentation:
#
#     https://docs.scrapy.org/en/latest/topics/settings.html
#     https://docs.scrapy.org/en/latest/topics/downloader-middleware.html
#     https://docs.scrapy.org/en/latest/topics/spider-middleware.html

from os import path
import copy

from colorlog import ColoredFormatter
import scrapy.utils.log

BOT_NAME = 'film_scraper'

SPIDER_MODULES = ['film_scraper.spiders']
NEWSPIDER_MODULE = 'film_scraper.spiders'


# Crawl responsibly by identifying yourself (and your website) on the user-agent
USER_AGENT = "film_scraper"

# Obey robots.txt rules
ROBOTSTXT_OBEY = True

# Global variables
NUM_SITEMAP = 1             # номер карты с ссылками

PATH_2_WORK_DIR = "D:/Мои документы/Лабы и рефераты/5 курс 1 семестр/Информационный поиск/Сбор документов/film_scraper/"

PATH_2_JSONS = path.join(PATH_2_WORK_DIR, "film_scraper/jsons")
PATH_2_FILMS_JSON =  path.join(PATH_2_JSONS, "films{:d}.jsonlines".format(NUM_SITEMAP))
PATH_2_ARTICLES_JSON =  path.join(PATH_2_JSONS, "content{:d}.jsonlines".format(NUM_SITEMAP))

PATH_2_SITEMAPS = path.join(PATH_2_WORK_DIR, "film_scraper/sitemaps")
PATH_2_SITEMAP = path.join(PATH_2_SITEMAPS, "sitemap_film_review{:d}.txt".format(NUM_SITEMAP))

PATH_2_CAPTCHAS = path.join(PATH_2_WORK_DIR, "film_scraper/captcha")
PATH_2_CAPTCHA = path.join(PATH_2_CAPTCHAS, "sitemap_captcha_links{:d}.txt".format(NUM_SITEMAP))
PATH_2_DRIVER = "D:/Program Files/Chrome/chromedriver.exe"

IBEGIN = 1               # Индекс начала обработки
IEND = 10                # Индекс конца обработки

REVIEWS_OFFSET = 200           # Количество комментариев на одной странице

MINUTES_SLEEP_PER_HOUR = 5     # На сколько минут паук отключается в час

DOWNLOAD_DELAY = 1.1           # Задержка при скачивании

# Feed
FEED_EXPORT_ENCODING = 'utf-8'
FEED_FORMAT = 'jsonlines'
        
# Coloring log

color_formatter = ColoredFormatter(
    (
        '%(log_color)s%(levelname)-5s%(reset)s '
        '%(yellow)s[%(asctime)s]%(reset)s'
        '%(white)s %(name)s %(funcName)s %(bold_purple)s:%(lineno)d%(reset)s '
        '%(log_color)s%(message)s%(reset)s'
    ),
    datefmt='%y-%m-%d %H:%M:%S',
    log_colors={
        'DEBUG': 'blue',
        'INFO': 'bold_cyan',
        'WARNING': 'red',
        'ERROR': 'bg_bold_red',
        'CRITICAL': 'red,bg_white',
    }
)

_get_handler = copy.copy(scrapy.utils.log._get_handler)

def _get_handler_custom(*args, **kwargs):
    handler = _get_handler(*args, **kwargs)
    handler.setFormatter(color_formatter)
    return handler

scrapy.utils.log._get_handler = _get_handler_custom

# Configure maximum concurrent requests performed by Scrapy (default: 16)
#CONCURRENT_REQUESTS = 32

# Configure a delay for requests for the same website (default: 0)
# See https://docs.scrapy.org/en/latest/topics/settings.html#download-delay
# See also autothrottle settings and docs

# The download delay setting will honor only one of:
#CONCURRENT_REQUESTS_PER_DOMAIN = 16
CONCURRENT_REQUESTS_PER_IP = 16

# Disable cookies (enabled by default)
#COOKIES_ENABLED = False

# Disable Telnet Console (enabled by default)
#TELNETCONSOLE_ENABLED = False

# Override the default request headers:
#DEFAULT_REQUEST_HEADERS = {
#   'Accept': 'text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8',
#   'Accept-Language': 'en',
#}

# Enable or disable spider middlewares
# See https://docs.scrapy.org/en/latest/topics/spider-middleware.html
#SPIDER_MIDDLEWARES = {
#    'film_scraper.middlewares.FilmScraperSpiderMiddleware': 543,
#}

# Enable or disable downloader middlewares
# See https://docs.scrapy.org/en/latest/topics/downloader-middleware.html
#DOWNLOADER_MIDDLEWARES = {
#    'film_scraper.middlewares.FilmScraperDownloaderMiddleware': 543,
#}

# Enable or disable extensions
# See https://docs.scrapy.org/en/latest/topics/extensions.html
#EXTENSIONS = {
#    'scrapy.extensions.telnet.TelnetConsole': None,
#}

# Configure item pipelines
# See https://docs.scrapy.org/en/latest/topics/item-pipeline.html
#ITEM_PIPELINES = {
#    'film_scraper.pipelines.FilmScraperPipeline': 300,
#}

# Enable and configure the AutoThrottle extension (disabled by default)
# See https://docs.scrapy.org/en/latest/topics/autothrottle.html
#AUTOTHROTTLE_ENABLED = True
# The initial download delay
#AUTOTHROTTLE_START_DELAY = 5
# The maximum download delay to be set in case of high latencies
#AUTOTHROTTLE_MAX_DELAY = 60
# The average number of requests Scrapy should be sending in parallel to
# each remote server
#AUTOTHROTTLE_TARGET_CONCURRENCY = 1.0
# Enable showing throttling stats for every response received:
#AUTOTHROTTLE_DEBUG = False

# Enable and configure HTTP caching (disabled by default)
# See https://docs.scrapy.org/en/latest/topics/downloader-middleware.html#httpcache-middleware-settings
#HTTPCACHE_ENABLED = True
#HTTPCACHE_EXPIRATION_SECS = 0
#HTTPCACHE_DIR = 'httpcache'
#HTTPCACHE_IGNORE_HTTP_CODES = []
#HTTPCACHE_STORAGE = 'scrapy.extensions.httpcache.FilesystemCacheStorage'
