# URL Shortener

A production-style URL shortener written in C++ with the [Drogon](https://github.com/drogonframework/drogon) framework. The aim is to practice real backend work—REST APIs, auth, caching, databases, Docker, and designing something that could actually scale.

## Goal

Build a fast URL shortening service that can handle heavy traffic, and treat each milestone as a chance to apply solid backend engineering practices rather than just shipping features.

Work is incremental: one concept at a time, grounded in how production systems are usually built.

## Planned Features

### Authentication

- User registration and login
- JWT-based auth
- Secure password hashing

### URL management

- Create short URLs (with optional custom aliases)
- Expiration dates
- Update, delete, and search URLs
- Pagination for listing results

### Analytics

- Click counts
- Browser, device, and geographic breakdowns
- Referrer tracking

### Performance

- Redis caching
- Database indexing
- Connection pooling
- Rate limiting

### Developer experience

- REST API with OpenAPI (Swagger) docs
- Docker support
- Unit and integration tests
- CI/CD pipeline

## Tech Stack

| Category         | Technology              |
| ---------------- | ----------------------- |
| Language         | C++20                   |
| Framework        | Drogon                  |
| Database         | PostgreSQL              |
| Cache            | Redis                   |
| Authentication   | JWT                     |
| Build System     | CMake                   |
| Package Manager  | vcpkg                   |
| Containerization | Docker & Docker Compose |
| Version Control  | Git & GitHub            |
| API Testing      | Postman / Bruno         |

## Project Structure

```text
url-shortener/
├── src/
├── include/
├── config/
├── database/
├── docker/
├── docs/
├── scripts/
├── tests/
├── CMakeLists.txt
├── vcpkg.json
├── .gitignore
└── README.md
```

## Roadmap

- [ ] Project setup
- [ ] Database design
- [ ] User authentication
- [ ] URL shortening
- [ ] URL redirection
- [ ] Custom aliases
- [ ] URL expiration
- [ ] Analytics
- [ ] Redis integration
- [ ] Rate limiting
- [ ] Dockerization
- [ ] Testing
- [ ] Deployment

## What I'm Learning

This project is a way to get better at:

- Modern C++
- Backend and REST API design
- Database design
- Auth and authorization
- Caching
- Software architecture and system design
- Building backends that can scale

## License

Built for learning, portfolio work, and interview prep.
