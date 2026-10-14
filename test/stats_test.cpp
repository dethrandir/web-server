#include "stats/stats.hpp"

#include <catch_amalgamated.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace stats = web_server::stats;

TEST_CASE("stats: serializes a snapshot to JSON", "[stats]") {
  stats::Snapshot snapshot;
  snapshot.timestamp = "2026-01-01T00:00:00Z";
  snapshot.requests = 10;
  snapshot.responses_2xx = 7;
  snapshot.cache_hits = 3;
  snapshot.cache_misses = 1;
  snapshot.cache_hit_ratio = 0.75;
  snapshot.workers_total = 4;

  const std::string compact = stats::to_json(snapshot, /*pretty=*/false);
  REQUIRE(compact.find('\n') == std::string::npos);

  const nlohmann::json json = nlohmann::json::parse(compact);
  REQUIRE(json["requests"]["total"] == 10);
  REQUIRE(json["requests"]["2xx"] == 7);
  REQUIRE(json["cache"]["hits"] == 3);
  REQUIRE(json["cache"]["hit_ratio"] == Catch::Approx(0.75));
  REQUIRE(json["workers"]["total"] == 4);
}

TEST_CASE("stats: probes report plausible process values", "[stats]") {
  REQUIRE(stats::read_rss_bytes() > 0);
  REQUIRE(stats::read_process_cpu_seconds() >= 0.0);

  const std::string timestamp = stats::utc_timestamp();
  REQUIRE(timestamp.size() == 20);
  REQUIRE(timestamp.back() == 'Z');
  REQUIRE(timestamp[10] == 'T');
}
