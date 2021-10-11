# Define here the models for your scraped items
#
# See documentation in:
# https://docs.scrapy.org/en/latest/topics/items.html

import scrapy


class FilmScraperItem(scrapy.Item):
    page_url = scrapy.Field()
    title = scrapy.Field()
    alternative_title = scrapy.Field()
    year = scrapy.Field()
    score = scrapy.Field()
    countries = scrapy.Field()
    directors = scrapy.Field()
    screenwriters = scrapy.Field()
    actors = scrapy.Field()
    genres = scrapy.Field()
    description = scrapy.Field()
    reviews = scrapy.Field()
