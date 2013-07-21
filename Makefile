all:
	$(MAKE) -C example
	$(MAKE) -C redismq

clean:
	$(MAKE) -C example clean
	$(MAKE) -C redismq clean
