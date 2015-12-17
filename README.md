HttpUtils
=========

A collection of utilities which enable [express.js](http://expressjs.com/)-style of web server implementation for C++11.

### Components

* Port of [path-to-regexp](https://www.npmjs.com/package/path-to-regexp) Node.js library to C++11
* C++11 implementation of express.js-like routing interface

### Dependencies

* C++11
* boost::variant
* std::regex

### Building

Compile with a C++11 compilant compiler:
```
cmake .
make
```

### Running tests

HttpUtils contains unit test suite based on [Catch](https://github.com/philsquared/Catch) framework.

Run unit tests: `./httputilstest`

### Contact

Dmitri Rubinstein,
German Research Center for Artificial Intelligence (DFKI), Saarbruecken

e-mail: dmitri.rubinstein@dfki.de
