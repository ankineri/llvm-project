// RUN: %check_clang_tidy %s bugprone-unused-ntd-object %t
namespace absl {
class Status {
public:
  bool ok() {return true;}
};
}
bool simple_used_value() {
  absl::Status status;
  return status.ok();
}

bool if_used_value() {
  absl::Status status;
  if (status.ok()) {
    return true;
  }
  return false;
}

void accepts_status(absl::Status status) {
}

void used_by_function() {
  absl::Status status;
  accepts_status(status);
}

int value;
int& accepts_status_returns_ref(absl::Status status) {
  return value;
}

int* accepts_status_returns_ptr(absl::Status status) {
  return &value;
}


void used_assign_lhs() {
  absl::Status for_ref;
  accepts_status_returns_ref(for_ref) = 7;
  absl::Status for_ptr;
  *accepts_status_returns_ptr(for_ptr) = 42;
}

void unused_simple() {
  absl::Status unused;
// CHECK-MESSAGES: :[[@LINE-1]]:16: warning: 'unused' is unlikely to be RAII and is potentially unused [bugprone-unused-ntd-object]
}

void unused_reassigned() {
  absl::Status unused;
// CHECK-MESSAGES: :[[@LINE-1]]:16: warning: 'unused' is unlikely to be RAII and is potentially unused [bugprone-unused-ntd-object]
  unused = absl::Status();
}

void unused_checked_reassigned() {
  absl::Status unused;
// CHECK-MESSAGES: :[[@LINE-1]]:16: warning: 'unused' is unlikely to be RAII and is potentially unused [bugprone-unused-ntd-object]
  if (!unused.ok()) {
    return;
  }
  unused = absl::Status();
}
