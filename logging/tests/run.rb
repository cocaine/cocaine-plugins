require 'securerandom'
require 'cocaine'
require 'rspec'

Cocaine::LOG.level = Logger::DEBUG
Celluloid.logger.level = Cocaine::LOG.level


def timeout
  1
end

def new_logger(host: 'localhost', port: 10053)
#def new_logger(host = '2a02:6b8:0:1a71::3268', port = 10053)
  Cocaine::Service.new('logging', [[host, port]])
end

def ensure_log(frontend, msg, verbosity=1, attributes = [], headers = {})
  logger = new_logger()
  if headers.empty?
    tx, rx = logger.get(frontend)
  else
    tx, rx = logger.get(frontend, **headers)
  end

  tx.emit_ack(verbosity, msg, attributes)
  res = rx.recv(timeout)
  tx.emit_ack(verbosity, msg, attributes)
  res = rx.recv(timeout)
  expect(res[0]). to eq :write
  expect(res[1][0]). to be true
  logger.terminate
end

def ensure_not_log(frontend, msg, verbosity=1, attributes = [])
  logger = new_logger()
  tx, rx = logger.get(frontend)
  tx.emit_ack(verbosity, msg, attributes)
  res = rx.recv(timeout)
  expect(res[0]). to eq :write
  expect(res[1][0]). to be false
  logger.terminate
end

def log(frontend, msg, verbosity=1, attributes = [[]])
  logger = new_logger()
  tx, rx = logger.get(frontend)
  tx.emit(msg, verbosity, attributes)
  logger.terminate
end

def set_filter(frontend, filter, ttl = 60)
  logger = new_logger()
  tx, rx = logger.set_filter(frontend, filter, ttl)
  result = rx.recv(timeout)
  expect(result[0]). to eq :value;
  expect(result[1][0]). to be_an(Integer);
  logger.terminate
  return result[1][0]
end

def random_backend(backend = 'random_backend')
  backend + '_' + SecureRandom.hex
end

def valid_msg
  'hi there'
end

def invalid_msg
  'ERROR! SHOULD NOT BE HERE IN LOG!'
end

describe :Logging do
  it 'should create named logger' do
    logger = new_logger()
    tx, rx = logger.get('test_logger')
  end

  it 'should log without attributes and verbosity via named logger' do
    logger = new_logger()
    tx, rx = logger.get('test_logger')
    tx.emit('simple log test')
  end

  it 'should log without attributes and with verbosity via named logger' do
    logger = new_logger()
    tx, rx = logger.get('test_logger')
    tx.emit('simple log with verbosity only test', 2)
  end

  it 'should log with verbosity and attributes via named logger' do
    log('test_logger','simple log with verbosity and attributes test', 2, [['test_attr1', 3], ['test_attr2', 'str_value'], ['test_attr3', 7.7], ['test_attr4', true]])
  end

  it 'should list loggers' do
    backend = random_backend('test_logger_to_list')
    set_filter(backend, ['empty'])
    ensure_log(backend, 'YAY')
    logger = new_logger()
    tx, rx = logger.list_loggers()
    result = rx.recv(timeout)
    expect(result[0]).to eq :value
    expect(result[1][0]).to include backend
  end

  it 'should correctly set not_exists filter' do
    backend = random_backend('not_exists_filter_test')
    set_filter(backend, ['!e', 'test_attr'])
    ensure_not_log(backend, invalid_msg, 2, [['test_attr', 'here i am']])
    ensure_log(backend, 'not_exists test', 2, [['another_attr', 'hi there']])
  end

  it 'should correctly set exists filter' do
    backend = random_backend('exists_filter_test')
    set_filter(backend, ['e', 'another_attr'])
    ensure_not_log(backend, invalid_msg, 2, [['test_attr', 'here i am']])
    ensure_log(backend, 'exists test', 2, [['another_attr', 'hi there']])
  end

  it 'should correctly set equals filter for string' do
    backend = random_backend('string_equals_filter_test')
    set_filter(backend, ['==', 'another_attr', 'hi there'])
    ensure_not_log(backend, invalid_msg, 2, [['another_attr', 'here i am']])
    ensure_log(backend, 'string equals test', 2, [['another_attr', 'hi there']])
  end

  it 'should correctly set equals filter for int' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['==', 'another_attr', 42])
    ensure_log(backend, 'int equals test', 2, [['another_attr', 42]])
    ensure_not_log(backend, invalid_msg, 2, [['another_attr', 'hi there']])
    ensure_log(backend, 'int equals test', 2, [['another_attr', 42.7]])
  end

  it 'should correctly set equals filter for double' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['==', 'another_attr', 42.0])
    ensure_log(backend, 'double equals test', 2, [['another_attr', 42.0]])
    ensure_not_log(backend, 'double equals test', 2, [['another_attr', 42.1]])
    ensure_log(backend, 'double equals test', 2, [['another_attr', 42]])
    ensure_not_log(backend, 'double equals test', 2, [['another_attr', 43]])
  end

  it 'should correctly set equals filter for bool' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['==', 'another_attr', true])
    ensure_log(backend, 'bool equals test', 2, [['another_attr', true]])
    ensure_not_log(backend, 'bool equals test', 2, [['another_attr', false]])
    ensure_not_log(backend, 'bool equals test', 2, [['another_attr', 0]])
    ensure_not_log(backend, 'bool equals test', 2, [['another_attr', 0.0]])
    ensure_not_log(backend, 'bool equals test', 2, [['another_attr', 'STR']])
  end

  it 'should correctly set equals filter for bool with conversion' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['==', 'another_attr', true])

  end

  it 'should correctly set not_equals filter for string' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['!=', 'another_attr', 'test_str'])
    ensure_log(backend, 'str not equals test', 2, [['another_attr', 1]])
    ensure_log(backend, 'str not equals test', 2, [['another_attr', 1.0]])
    ensure_log(backend, 'str not equals test', 2, [['another_attr', 'STR']])
    ensure_log(backend, 'str not equals test', 2, [['another_attr', 0]])
    ensure_not_log(backend, 'str not equals test', 2, [['another_attr', 'test_str']])
  end

  it 'should correctly set not_equals filter for int' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['!=', 'another_attr', 42])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 1]])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 1.0]])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 'STR']])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 0]])
    ensure_not_log(backend, 'int not equals test', 2, [['another_attr', 42.0]])
    ensure_not_log(backend, 'int not equals test', 2, [['another_attr', 42]])
  end

  it 'should correctly set not_equals filter for double' do
    backend = random_backend('int_equals_filter_test')
    set_filter(backend, ['!=', 'another_attr', 42.0])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 1]])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 1.0]])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 'STR']])
    ensure_log(backend, 'int not equals test', 2, [['another_attr', 0]])
    ensure_not_log(backend, 'int not equals test', 2, [['another_attr', 42.0]])
    ensure_not_log(backend, 'int not equals test', 2, [['another_attr', 42]])
  end

  it 'should correctly list all filters' do
    filter = ['==', 'another_attr', 42.0]
    back1 = random_backend
    back2 = random_backend
    back3 = random_backend
    id1 = set_filter(back1, filter)
    id2 = set_filter(back2, filter)
    id3 = set_filter(back3, filter)
    logger = new_logger()
    tx, rx = logger.list_filters()
    res = rx.recv(timeout)
    expect(res[0]).to eq :value
    mapped = res[1][0].map! {|x| x.delete_at(3); x}
    expect(mapped).to include [back1, filter, id1,  0]
    expect(mapped).to include [back2, filter, id2, 0]
    expect(mapped).to include [back3, filter, id3, 0]
  end

  it 'should correctly set cluster filter' do
    backend = random_backend('cluster_filter_test')
    logger = new_logger(port: 10054)
    tx, rx = logger.set_cluster_filter(backend, ['==', 'another_attr', 42], 60)
    ensure_not_log(backend, invalid_msg, 2, [['another_attr', 1]])
  end

  it 'should correctly handle prefixes' do
    backend = random_backend('prefixes_test')
    logger = new_logger()
    tx, rx = logger.set_filter(backend, ['severity', 0], 60)
    bigger_backend = backend + random_backend('')
    ensure_log(bigger_backend, 'prefix test', 2, [])
  end

  it 'should correctly handle trace_bit filter' do
    backend = random_backend('trace_bit_test')
    logger = new_logger()
    tx, rx = logger.set_filter(backend, ['traced'], 60)
    ensure_log(backend, 'trace_bit_test', 0, [], {trace_bit: '1', trace_id: 'aaaaaaaa', span_id: 'aaaaaaaa', parent_id: 'aaaaaaaa'})
    ensure_not_log(backend, invalid_msg, 0, [])
  end
end
