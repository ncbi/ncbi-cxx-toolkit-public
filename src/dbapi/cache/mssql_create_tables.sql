CREATE TABLE dbo.cache_data
(
   cache_key     varchar(256) NOT NULL,
   version       int          NOT NULL,
   subkey        varchar(256) NOT NULL,
   
   data          image        NULL

   CONSTRAINT cache_data_pk PRIMARY KEY CLUSTERED
              (cache_key, version, subkey)
);

CREATE TABLE dbo.cache_attr
(
   cache_key        varchar(256) NOT NULL,
   version          int          NOT NULL,
   subkey           varchar(256) NOT NULL,
   
   cache_timestamp  int

   CONSTRAINT cache_attr_pk PRIMARY KEY CLUSTERED
              (cache_key, version, subkey)
);

  